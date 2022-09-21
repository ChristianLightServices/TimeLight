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

    template<Stage stage>
    Result runStage() const
    {
        return Result::Valid;
    }
    template<>
    Result runStage<Stage::TimeService>() const;
    template<>
    Result runStage<Stage::ApiKey>() const;
    template<>
    Result runStage<Stage::Workspace>() const;
    template<>
    Result runStage<Stage::Project>() const;

    template<Stage stage>
    void resetStage() const
    {}
    template<>
    void resetStage<Stage::TimeService>() const;
    template<>
    void resetStage<Stage::ApiKey>() const;
    template<>
    void resetStage<Stage::Workspace>() const;
    template<>
    void resetStage<Stage::Project>() const;

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
