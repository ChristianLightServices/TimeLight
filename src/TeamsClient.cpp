#include "TeamsClient.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuthHttpServerReplyHandler>
#include <QUrlQuery>

#if __has_include(<keychain.h>)
    #include <keychain.h>
#else
    #include <qt6keychain/keychain.h>
#endif

#include "JsonHelper.h"
#include "Logger.h"

namespace logs = TimeLight::logs;
using nlohmann::json;

TeamsClient::TeamsClient(const QString &appId, int port, QObject *parent)
    : QObject{parent},
      m_appId{appId},
      m_oauth{new QOAuth2AuthorizationCodeFlow{this}}
{
    m_manager.setAutoDeleteReplies(true);
    m_oauth->setNetworkAccessManager(&m_manager);

    auto handler{new QOAuthHttpServerReplyHandler{static_cast<quint16>(port), m_oauth}};
    handler->setCallbackPath("TimeLight/auth/microsoft/graph");
    handler->setCallbackText(
        QStringLiteral(R"(<p style="font-family:sans;text-align:center;font-size:35px;margin-top:30%">%1</p>)")
            .arg(tr("TimeLight has successfully authenticated with Teams. You may now close this tab.")));
    m_oauth->setReplyHandler(handler);
    m_oauth->setAuthorizationUrl(QStringLiteral("https://login.microsoftonline.com/organizations/oauth2/v2.0/"
                                                "authorize?response_type=code"));
    m_oauth->setAccessTokenUrl(QStringLiteral("https://login.microsoftonline.com/organizations/oauth2/v2.0/token"));
    m_oauth->setScope(QStringLiteral("user.read presence.readwrite offline_access"));
    m_oauth->setClientIdentifier(m_appId);
    m_oauth->setContentType(QAbstractOAuth2::ContentType::Json);

    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, &QDesktopServices::openUrl);
    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::granted, this, [this] {
        m_authenticated = true;
        m_authenticationInProgress = false;
        m_refreshingTokens = false;

        emit authenticatedChanged();
        emit accessTokenChanged();
        emit refreshTokenChanged();

        // refresh 15 seconds ahead of time
        m_refreshTokenTimer.setInterval(QDateTime::currentDateTime().msecsTo(m_oauth->expirationAt()) - 15 * 1000);
        m_refreshTokenTimer.start();

        logs::teams()->trace("Successfully authenticated with Graph");
    });
    connect(m_oauth,
            &QOAuth2AuthorizationCodeFlow::error,
            this,
            [this](const QString &error, const QString &desc, const QUrl &url) {
                logs::teams()->error("OAuth error: {}", error.toStdString());

                // the error may have been caused by a bad refresh attempt
                if (m_refreshingTokens)
                {
                    logs::teams()->debug("Attempting to counteract bad refresh attempt");
                    m_refreshingTokens = false;
                    m_oauth->grant();
                }
                else
                    m_authenticationInProgress = false;
            });
    m_oauth->setModifyParametersFunction([this](QOAuth2AuthorizationCodeFlow::Stage s, QMultiMap<QString, QVariant> *v) {
        // apparently this is necessary to make Graph work
        if (s == QOAuth2AuthorizationCodeFlow::Stage::RefreshingAccessToken)
            v->insert("scope", QStringLiteral("user.read presence.readwrite offline_access"));
    });

    // Graph API tokens expire every 60-90 minutes; by default, we'll keep the access token up-to-date all the time
    m_refreshTokenTimer.setInterval(m_oauth->expirationAt().isValid() ?
                                        QDateTime::currentDateTime().msecsTo(m_oauth->expirationAt()) - 60000 :
                                        1000 * 60 * 59);
    m_refreshTokenTimer.setSingleShot(false);
    m_refreshTokenTimer.callOnTimeout(m_oauth, &QOAuth2AuthorizationCodeFlow::refreshAccessToken);
}

void TeamsClient::authenticate()
{
    if (m_authenticationInProgress)
        return;
    m_authenticationInProgress = true;

    if (!m_oauth->refreshToken().isEmpty())
    {
        m_refreshingTokens = true;
        logs::teams()->trace("Refreshing access token");
        m_oauth->refreshAccessToken();
    }
    else
    {
        logs::teams()->trace("Attempting to authenticate with Graph");
        m_oauth->grant();
    }
}

void TeamsClient::clearPresence()
{
    QNetworkRequest req{QUrl{QStringLiteral("https://graph.microsoft.com/v1.0/me/presence/clearPresence")}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", "Bearer " + m_oauth->token().toUtf8());

    json j{{"sessionId", m_appId}};
    auto rep = m_manager.post(req, QByteArray::fromStdString(j.dump()));

    bool *done = new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, done] {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 200)
                [[likely]];
            else
            {
                logs::teams()->error("Failed to clear presence (HTTP responce code {})", status);
                logs::teams()->debug("Response data: '{}'", rep->readAll().toStdString());
            }

            if (done)
                *done = true;
        },
        Qt::DirectConnection);

    while (!(*done))
    {
        qApp->processEvents();
        qApp->sendPostedEvents();
    }

    if (done)
        delete done;
}

void TeamsClient::setPresence(const TeamsClient::Presence p)
{
    if (m_presence == p)
        return;

    QNetworkRequest req{QUrl{QStringLiteral("https://graph.microsoft.com/v1.0/me/presence/setPresence")}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", "Bearer " + m_oauth->token().toUtf8());

    json j{{"sessionId", m_appId}, {"expirationDuration", "PT4H"}};
    switch (p)
    {
    case Presence::Available:
        j["availability"] = "Available";
        j["activity"] = "Available";
        break;
    case Presence::Away:
        j["availability"] = "Away";
        j["activity"] = "Away";
        break;
    case Presence::InACall:
        j["availability"] = "Busy";
        j["activity"] = "InACall";
        break;
    case Presence::InAConferenceCall:
        j["availability"] = "Busy";
        j["activity"] = "InAConferenceCall";
        break;
    default:
        break;
    }

    auto rep = m_manager.post(req, QByteArray::fromStdString(j.dump()));

    bool *done = new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, done, p] {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 200) [[likely]]
                m_presence = p;
            else
            {
                logs::teams()->error("Failed to set presence (HTTP responce code {})", status);
                logs::teams()->debug("Response data: '{}'", rep->readAll().toStdString());
            }

            if (done)
                *done = true;
        },
        Qt::DirectConnection);

    while (!(*done))
    {
        qApp->processEvents();
        qApp->sendPostedEvents();
    }

    if (done)
        delete done;
}
