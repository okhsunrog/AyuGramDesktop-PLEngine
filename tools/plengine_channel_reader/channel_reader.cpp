#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>

#include "scheme.h"

#include "rpl/producer.h"

#include "mtproto/mtp_instance.h"

#include "data/data_peer_id.h"
#include "base/weak_ptr.h"

class UserData;
class PeerData;
struct TextWithEntities;

#include "main/main_session.h"

#include "plengine/plugin_api.h"

static_assert(sizeof(MemData) == 32);
static_assert(offsetof(AyuPlugin, memData) == 2048);
static_assert(sizeof(AyuPlugin) == 2080);

namespace {

struct Config {
  bool enabled = true;
  QStringList channels;
  int historyLimit = 100;
  int pollIntervalSeconds = 60;
  QString outputPath;
};

struct ResolvedChannel {
  std::uint64_t id = 0;
  std::uint64_t accessHash = 0;
  QString username;
  QString title;
};

struct Link {
  QString kind;
  QString url;
  QString text;
};

struct Post {
  std::uint64_t channelId = 0;
  QString channelUsername;
  QString channelTitle;
  int messageId = 0;
  int postedAt = 0;
  std::optional<int> editedAt;
  QString text;
  std::vector<Link> links;
};

auto Plugin = AyuPlugin{
    L"PLEngine Channel Reader",
    L"Reads posts from configured public channels into a local JSONL feed.",
    L"ayu-plengine-channel-reader.so",
    {},
};

std::unordered_map<std::string, ResolvedChannel> Resolved;
std::unordered_set<std::string> SeenPosts;
bool PollInProgress = false;

[[nodiscard]] auto DataDirectory() -> QString {
  auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (path.isEmpty()) {
    path = QDir::home().filePath(u".local/share/AyuGramDesktop"_q);
  }
  return QDir(path).filePath(u"plugins"_q);
}

[[nodiscard]] auto DefaultOutputPath() -> QString {
  return QDir(DataDirectory()).filePath(u"channel-reader-posts.jsonl"_q);
}

[[nodiscard]] auto StatusPath() -> QString {
  return QDir(DataDirectory()).filePath(u"channel-reader-status.json"_q);
}

[[nodiscard]] auto ConfigPath() -> QString {
  if (const auto overridePath = std::getenv("AYUGRAM_CHANNEL_READER_CONFIG");
      overridePath && *overridePath) {
    return QString::fromLocal8Bit(overridePath);
  }
  auto path =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (path.isEmpty()) {
    path = QDir::home().filePath(u".config/AyuGramDesktop"_q);
  }
  return QDir(path).filePath(u"channel-reader.json"_q);
}

void EnsureParentDirectory(const QString &path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
}

void WriteStatus(const QString &state, const QString &message,
                 int appended = 0) {
  const auto path = StatusPath();
  EnsureParentDirectory(path);
  auto file = QSaveFile(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return;
  }
  auto object = QJsonObject{
      {u"schema_version"_q, 1},
      {u"state"_q, state},
      {u"message"_q, message},
      {u"appended"_q, appended},
      {u"updated_at"_q, QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
      {u"config_path"_q, ConfigPath()},
  };
  file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
  file.write("\n");
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  file.commit();
}

[[nodiscard]] auto NormalizeUsername(QString username)
    -> std::optional<QString> {
  username = username.trimmed();
  if (username.startsWith('@')) {
    username.remove(0, 1);
  }
  if (username.isEmpty() || username.size() > 32) {
    return std::nullopt;
  }
  for (const auto character : username) {
    const auto value = character.unicode();
    const auto asciiLetter =
        (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
    if (!asciiLetter && (value < '0' || value > '9') && value != '_') {
      return std::nullopt;
    }
  }
  return username;
}

[[nodiscard]] auto LoadConfig() -> std::optional<Config> {
  const auto path = ConfigPath();
  auto file = QFile(path);
  if (!file.open(QIODevice::ReadOnly)) {
    WriteStatus(u"waiting_for_config"_q,
                u"Create the channel reader config at "_q + path);
    return std::nullopt;
  }
  auto parseError = QJsonParseError();
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    WriteStatus(u"config_error"_q,
                u"Invalid JSON: "_q + parseError.errorString());
    return std::nullopt;
  }

  const auto object = document.object();
  auto result = Config();
  result.enabled = object.value(u"enabled"_q).toBool(true);
  result.historyLimit =
      std::clamp(object.value(u"history_limit"_q).toInt(100), 1, 100);
  result.pollIntervalSeconds =
      std::clamp(object.value(u"poll_interval_seconds"_q).toInt(60), 10, 86400);
  result.outputPath = object.value(u"output_path"_q).toString().trimmed();
  if (result.outputPath.isEmpty()) {
    result.outputPath = DefaultOutputPath();
  }

  const auto channels = object.value(u"channels"_q).toArray();
  for (const auto &entry : channels) {
    const auto normalized = NormalizeUsername(entry.toString());
    if (normalized &&
        !result.channels.contains(*normalized, Qt::CaseInsensitive)) {
      result.channels.push_back(*normalized);
    }
  }
  if (result.enabled && result.channels.isEmpty()) {
    WriteStatus(u"config_error"_q,
                u"The enabled config has no valid channel usernames"_q);
    return std::nullopt;
  }
  return result;
}

template <typename Result, typename Done>
[[nodiscard]] auto ResponseHandlers(Done done,
                                    std::function<void(QString)> fail)
    -> MTP::ResponseHandler {
  auto onDone = [done =
                     std::move(done)](const MTP::Response &response) mutable {
    auto result = Result();
    auto from = response.reply.constData();
    if (!result.read(from, from + response.reply.size())) {
      WriteStatus(u"protocol_error"_q,
                  u"Could not decode an MTProto response"_q);
      return true;
    }
    done(result);
    return true;
  };
  auto onFail = [fail = std::move(fail)](const MTP::Error &error,
                                         const MTP::Response &) mutable {
    fail(error.type());
    return true;
  };
  return MTP::ResponseHandler{std::move(onDone), std::move(onFail)};
}

[[nodiscard]] auto Session() -> Main::Session * {
  return reinterpret_cast<Main::Session *>(Plugin.memData.activeSessionPtr);
}

void ScheduleNextPoll(int seconds);

void AddLink(std::vector<Link> &links, QString kind, QString url,
             QString text = QString()) {
  kind = kind.trimmed();
  url = url.trimmed();
  text = text.trimmed();
  if (kind.isEmpty() || url.isEmpty()) {
    return;
  }
  const auto duplicate = std::ranges::find_if(links, [&](const Link &link) {
    return link.kind == kind && link.url == url && link.text == text;
  });
  if (duplicate == links.end()) {
    links.push_back({
        .kind = std::move(kind),
        .url = std::move(url),
        .text = std::move(text),
    });
  }
}

void AppendEntityLinks(std::vector<Link> &links, const QString &text,
                       const QList<MTPMessageEntity> &entities) {
  for (const auto &entity : entities) {
    entity.match(
        [&](const MTPDmessageEntityUrl &data) {
          const auto visible =
              text.mid(data.voffset().v, data.vlength().v).trimmed();
          AddLink(links, u"text_url"_q, visible, visible);
        },
        [&](const MTPDmessageEntityTextUrl &data) {
          AddLink(links, u"hidden_text_url"_q, qs(data.vurl()),
                  text.mid(data.voffset().v, data.vlength().v));
        },
        [&](const MTPDmessageEntityEmail &data) {
          const auto email =
              text.mid(data.voffset().v, data.vlength().v).trimmed();
          AddLink(links, u"email"_q, u"mailto:"_q + email, email);
        },
        [&](const auto &) {});
  }
}

void AppendButtonLinks(std::vector<Link> &links, const MTPReplyMarkup &markup) {
  markup.match(
      [&](const MTPDreplyInlineMarkup &data) {
        for (const auto &row : data.vrows().v) {
          for (const auto &button : row.data().vbuttons().v) {
            const auto &fields = button.data();
            fields.vtype().match(
                [&](const MTPDinlineButtonTypeUrl &type) {
                  AddLink(links, u"inline_button"_q, qs(type.vurl()),
                          qs(fields.vtext()));
                },
                [&](const MTPDinlineButtonTypeUrlAuth &type) {
                  AddLink(links, u"inline_auth_button"_q, qs(type.vurl()),
                          qs(fields.vtext()));
                },
                [&](const MTPDinlineButtonTypeWebView &type) {
                  AddLink(links, u"inline_webview_button"_q, qs(type.vurl()),
                          qs(fields.vtext()));
                },
                [&](const auto &) {});
          }
        }
      },
      [&](const auto &) {});
}

void AppendWebPageLinks(std::vector<Link> &links, const MTPMessageMedia &media) {
  media.match(
      [&](const MTPDmessageMediaWebPage &data) {
        data.vwebpage().match(
            [&](const MTPDwebPage &page) {
              AddLink(links, u"webpage"_q, qs(page.vurl()),
                      qs(page.vtitle().value_or_empty()));
            },
            [&](const MTPDwebPageEmpty &page) {
              if (const auto url = page.vurl()) {
                AddLink(links, u"webpage"_q, qs(*url));
              }
            },
            [&](const MTPDwebPagePending &page) {
              if (const auto url = page.vurl()) {
                AddLink(links, u"webpage"_q, qs(*url));
              }
            },
            [&](const auto &) {});
      },
      [&](const auto &) {});
}

void AppendPosts(const Config &config, std::vector<Post> posts) {
  std::ranges::sort(posts, {}, [](const Post &post) {
    return std::pair(post.postedAt, post.messageId);
  });

  auto output = QByteArray();
  auto appended = 0;
  for (const auto &post : posts) {
    auto signature = QByteArray();
    signature += post.channelUsername.toLower().toUtf8();
    signature += ':';
    signature += QByteArray::number(post.messageId);
    signature += ':';
    signature += QByteArray::number(post.postedAt);
    signature += ':';
    signature += post.editedAt ? QByteArray::number(*post.editedAt) : "-";
    signature += ':';
    signature +=
        QCryptographicHash::hash(post.text.toUtf8(), QCryptographicHash::Sha256)
            .toHex();
    auto links = QJsonArray();
    for (const auto &link : post.links) {
      links.push_back(QJsonObject{
          {u"kind"_q, link.kind},
          {u"url"_q, link.url},
          {u"text"_q, link.text.isEmpty() ? QJsonValue(QJsonValue::Null)
                                          : QJsonValue(link.text)},
      });
    }
    signature += ':';
    signature +=
        QCryptographicHash::hash(QJsonDocument(links).toJson(QJsonDocument::Compact),
                                 QCryptographicHash::Sha256)
            .toHex();
    const auto key = signature.toStdString();
    if (!SeenPosts.emplace(key).second) {
      continue;
    }
    auto record = QJsonObject{
        {u"schema_version"_q, 2},
        {u"channel_id"_q, static_cast<qint64>(post.channelId)},
        {u"channel_username"_q, post.channelUsername},
        {u"channel_title"_q, post.channelTitle},
        {u"message_id"_q, post.messageId},
        {u"posted_at"_q,
         QDateTime::fromSecsSinceEpoch(post.postedAt, QTimeZone::UTC)
             .toString(Qt::ISODate)},
        {u"text"_q, post.text},
        {u"links"_q, links},
        {u"source_url"_q, u"https://t.me/"_q + post.channelUsername + u"/"_q +
                              QString::number(post.messageId)},
    };
    record.insert(u"edited_at"_q,
                  post.editedAt ? QJsonValue(QDateTime::fromSecsSinceEpoch(
                                                 *post.editedAt, QTimeZone::UTC)
                                                 .toString(Qt::ISODate))
                                : QJsonValue(QJsonValue::Null));
    output += QJsonDocument(record).toJson(QJsonDocument::Compact);
    output += '\n';
    ++appended;
  }

  if (!output.isEmpty()) {
    EnsureParentDirectory(config.outputPath);
    auto file = QFile(config.outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
      WriteStatus(u"output_error"_q,
                  u"Could not append to "_q + config.outputPath);
      return;
    }
    if (file.write(output) != output.size()) {
      WriteStatus(u"output_error"_q,
                  u"Could not write the complete JSONL batch"_q);
      return;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }
  WriteStatus(u"ready"_q,
              u"Polling "_q + QString::number(config.channels.size()) +
                  u" configured channels"_q,
              appended);
}

[[nodiscard]] auto PostsFromResult(const MTPmessages_Messages &result,
                                   const ResolvedChannel &channel)
    -> std::vector<Post> {
  auto posts = std::vector<Post>();
  result.match([&](const auto &data) {
    using Data = std::decay_t<decltype(data)>;
    if constexpr (!MTPDmessages_messagesNotModified::Is<Data>()) {
      for (const auto &message : data.vmessages().v) {
        message.match([&](const auto &fields) {
          using Message = std::decay_t<decltype(fields)>;
          if constexpr (MTPDmessage::Is<Message>()) {
            const auto text = qs(fields.vmessage());
            if (!text.trimmed().isEmpty()) {
              auto links = std::vector<Link>();
              AppendEntityLinks(links, text, fields.ventities().value_or_empty());
              if (const auto markup = fields.vreply_markup()) {
                AppendButtonLinks(links, *markup);
              }
              if (const auto media = fields.vmedia()) {
                AppendWebPageLinks(links, *media);
              }
              posts.push_back(Post{
                  .channelId = channel.id,
                  .channelUsername = channel.username,
                  .channelTitle = channel.title,
                  .messageId = fields.vid().v,
                  .postedAt = fields.vdate().v,
                  .editedAt = fields.vedit_date()
                                  ? std::optional(fields.vedit_date()->v)
                                  : std::nullopt,
                  .text = text,
                  .links = std::move(links),
              });
            }
          }
        });
      }
    }
  });
  return posts;
}

void FetchChannel(const Config &config, const ResolvedChannel &channel,
                  std::function<void(std::vector<Post>)> done) {
  auto session = Session();
  if (!session) {
    WriteStatus(u"session_error"_q, u"AyuGram has no active session"_q);
    done({});
    return;
  }
  const auto request = MTPmessages_GetHistory(
      MTP_inputPeerChannel(MTP_long(channel.id), MTP_long(channel.accessHash)),
      MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(config.historyLimit),
      MTP_int(0), MTP_int(0), MTP_long(0));
  const auto completion =
      std::make_shared<std::function<void(std::vector<Post>)>>(std::move(done));
  session->mtp().sendReq(
      request,
      ResponseHandlers<MTPmessages_Messages>(
          [channel, completion](const MTPmessages_Messages &result) mutable {
            (*completion)(PostsFromResult(result, channel));
          },
          [username = channel.username, completion](QString error) mutable {
            WriteStatus(u"history_error"_q,
                        u"@"_q + username + u": "_q + error);
            (*completion)({});
          }));
}

void ResolveChannel(const QString &username,
                    std::function<void(std::optional<ResolvedChannel>)> done) {
  const auto key = username.toLower().toStdString();
  if (const auto found = Resolved.find(key); found != Resolved.end()) {
    done(found->second);
    return;
  }
  auto session = Session();
  if (!session) {
    WriteStatus(u"session_error"_q, u"AyuGram has no active session"_q);
    done(std::nullopt);
    return;
  }

  const auto request = MTPcontacts_ResolveUsername(
      MTP_flags(0), MTP_string(username), MTP_string());
  const auto completion =
      std::make_shared<std::function<void(std::optional<ResolvedChannel>)>>(
          std::move(done));
  session->mtp().sendReq(
      request, ResponseHandlers<MTPcontacts_ResolvedPeer>(
                   [key, username, completion](
                       const MTPcontacts_ResolvedPeer &result) mutable {
                     auto channel = std::optional<ResolvedChannel>();
                     result.match([&](const MTPDcontacts_resolvedPeer &data) {
                       auto resolvedId = std::uint64_t();
                       data.vpeer().match([&](const auto &peer) {
                         using Peer = std::decay_t<decltype(peer)>;
                         if constexpr (MTPDpeerChannel::Is<Peer>()) {
                           resolvedId = peer.vchannel_id().v;
                         }
                       });
                       for (const auto &chat : data.vchats().v) {
                         chat.match([&](const auto &fields) {
                           using Chat = std::decay_t<decltype(fields)>;
                           if constexpr (MTPDchannel::Is<Chat>()) {
                             const auto accessHash =
                                 fields.vaccess_hash().value_or_empty();
                             if (accessHash && fields.vid().v == resolvedId) {
                               channel = ResolvedChannel{
                                   .id = fields.vid().v,
                                   .accessHash = accessHash,
                                   .username = username,
                                   .title = qs(fields.vtitle()),
                               };
                             }
                           }
                         });
                       }
                     });
                     if (!channel) {
                       WriteStatus(u"resolve_error"_q,
                                   u"@"_q + username +
                                       u" is not an accessible channel"_q);
                     } else {
                       Resolved.insert_or_assign(key, *channel);
                     }
                     (*completion)(channel);
                   },
                   [username, completion](QString error) mutable {
                     WriteStatus(u"resolve_error"_q,
                                 u"@"_q + username + u": "_q + error);
                     (*completion)(std::nullopt);
                   }));
}

void PollChannels(std::shared_ptr<Config> config, int index,
                  std::shared_ptr<std::vector<Post>> posts) {
  if (index >= config->channels.size()) {
    AppendPosts(*config, std::move(*posts));
    PollInProgress = false;
    ScheduleNextPoll(config->pollIntervalSeconds);
    return;
  }
  const auto username = config->channels[index];
  ResolveChannel(username, [config, index,
                            posts](std::optional<ResolvedChannel> channel) {
    if (!channel) {
      PollChannels(config, index + 1, posts);
      return;
    }
    FetchChannel(
        *config, *channel, [config, index, posts](std::vector<Post> fetched) {
          posts->insert(posts->end(), std::make_move_iterator(fetched.begin()),
                        std::make_move_iterator(fetched.end()));
          PollChannels(config, index + 1, posts);
        });
  });
}

void Poll() {
  if (PollInProgress) {
    return;
  }
  const auto loaded = LoadConfig();
  if (!loaded) {
    ScheduleNextPoll(30);
    return;
  }
  if (!loaded->enabled) {
    WriteStatus(u"disabled"_q, u"Channel polling is disabled in config"_q);
    ScheduleNextPoll(loaded->pollIntervalSeconds);
    return;
  }
  PollInProgress = true;
  PollChannels(std::make_shared<Config>(*loaded), 0,
               std::make_shared<std::vector<Post>>());
}

void ScheduleNextPoll(int seconds) {
  QTimer::singleShot(std::chrono::seconds(seconds), [] { Poll(); });
}

} // namespace

extern "C" __attribute__((visibility("default"))) auto pluginInfo()
    -> AyuPlugin * {
  return &Plugin;
}

extern "C" __attribute__((visibility("default"))) void internalSetup() {
  if (const auto addToQueue = Plugin.memData.addToQueue) {
    addToQueue([] { Poll(); });
  } else {
    WriteStatus(u"setup_error"_q,
                u"PLEngine did not provide its main-thread queue"_q);
  }
}
