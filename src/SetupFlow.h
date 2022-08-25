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

public slots:
    void cancelFlow();
    Result runNextStage();
    Result rerunCurrentStage();
    Result runPreviousStage();

private:
    TrayIcons *m_parent;
    Stage m_stage{Stage::NotStarted};
};

#endif // SETUPFLOW_H
