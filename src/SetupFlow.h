#ifndef SETUPFLOW_H
#define SETUPFLOW_H

#include <QObject>

#include "TrayIcons.h"

class SetupFlow : public QObject
{
    Q_OBJECT

public:
    explicit SetupFlow(TrayIcons *parent);

    enum class Stage
    {
        NotStarted,

        TimeService,
        ApiKey,
        Workspace,
        Project,

        Done,
    };

    enum class Result
    {
        Valid,
        Invalid,
        Canceled,
    };

    bool done() const { return m_stage == Stage::Done; }
    Stage stage() const { return m_stage; }

    Result runTimeServiceStage() const;
    Result runApiKeyStage() const;
    Result runWorkspaceStage() const;
    Result runProjectStage() const;

    void resetTimeServiceStage() const;
    void resetApiKeyStage() const;
    void resetWorkspaceStage() const;
    void resetProjectStage() const;

public slots:
    void cancelFlow();

    SetupFlow::Result runNextStage();
    SetupFlow::Result runPreviousStage();
    SetupFlow::Result rerunCurrentStage();
    void resetNextStage();
    void resetPreviousStage();
    void resetCurrentStage();

private:
    TrayIcons *m_parent;
    Stage m_stage{Stage::NotStarted};
};

#endif // SETUPFLOW_H
