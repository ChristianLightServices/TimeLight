#ifndef TEAMSCLIENT_H
#define TEAMSCLIENT_H

#include <QNetworkAccessManager>
#include <QOAuth2AuthorizationCodeFlow>
#include <QObject>
#include <QTimer>

class TeamsClient : public QObject
{
    Q_OBJECT

public:
    enum class Presence
    {
        Available,
        InACall,
        InAConferenceCall,
        Away,
    };
    Q_ENUM(Presence)

    explicit TeamsClient(const QString &appId, int port = 6942, QObject *parent = nullptr);

    bool authenticated() const { return m_authenticated; }
    QString accessToken() const { return m_oauth->token(); }
    QString refreshToken() const { return m_oauth->refreshToken(); }

    Presence presence() const { return m_presence; }

signals:
    void authenticatedChanged();
    void accessTokenChanged();
    void refreshTokenChanged();

public slots:
    void authenticate();

    void setPresence(const TeamsClient::Presence p);
    void clearPresence();
    void setAccessToken(const QString &token) { m_oauth->setToken(token); }
    void setRefreshToken(const QString &token) { m_oauth->setRefreshToken(token); }

private:
    QNetworkAccessManager m_manager{this};

    const QString m_appId;

    QOAuth2AuthorizationCodeFlow *m_oauth;
    bool m_authenticated{false};
    bool m_refreshingTokens{false};
    bool m_authenticationInProgress{false};

    QTimer m_refreshTokenTimer;

    Presence m_presence{Presence::Available};
};

#endif // TEAMSCLIENT_H
