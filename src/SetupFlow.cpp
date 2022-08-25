#include "SetupFlow.h"

#include <QInputDialog>
#include <QMessageBox>

#include "ClockifyManager.h"
#include "TimeCampManager.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Logger.h"

namespace logs = TimeLight::logs;

SetupFlow::SetupFlow(TrayIcons *parent)
    : QObject{parent},
      m_parent{parent}
{}

void SetupFlow::cancelFlow()
{
    m_stage = Stage::NotStarted;
}

SetupFlow::Result SetupFlow::runNextStage()
{
    switch (m_stage)
    {
    case Stage::NotStarted:
        m_stage = Stage::TimeService;
        return runStage<Stage::TimeService>();
    case Stage::TimeService:
        m_stage = Stage::ApiKey;
        return runStage<Stage::ApiKey>();
    case Stage::ApiKey:
        m_stage = Stage::Workspace;
        return runStage<Stage::Workspace>();
    case Stage::Workspace:
        m_stage = Stage::Project;
        return runStage<Stage::Project>();
    case Stage::Project:
        m_stage = Stage::Done;
        return SetupFlow::Result::Valid;
    default:
        m_stage = Stage::NotStarted;
        return SetupFlow::Result::Valid;
    }
}

SetupFlow::Result SetupFlow::runPreviousStage()
{
    switch (m_stage)
    {
    case Stage::Done:
        m_stage = Stage::Project;
        return runStage<Stage::Project>();
    case Stage::Project:
        m_stage = Stage::Workspace;
        return runStage<Stage::Workspace>();
    case Stage::Workspace:
        m_stage = Stage::ApiKey;
        return runStage<Stage::ApiKey>();
    case Stage::ApiKey:
        m_stage = Stage::TimeService;
        return runStage<Stage::TimeService>();
    default:
        m_stage = Stage::NotStarted;
        return SetupFlow::Result::Valid;
    }
}

SetupFlow::Result SetupFlow::rerunCurrentStage()
{
    switch (m_stage)
    {
    case Stage::TimeService:
        return runStage<Stage::TimeService>();
    case Stage::ApiKey:
        return runStage<Stage::ApiKey>();
    case Stage::Workspace:
        return runStage<Stage::Workspace>();
    case Stage::Project:
        return runStage<Stage::Project>();
    default:
        return SetupFlow::Result::Valid;
    }
}

void SetupFlow::resetNextStage()
{
    switch (m_stage)
    {
    case Stage::Done:
        resetStage<Stage::TimeService>();
        break;
    case Stage::TimeService:
        resetStage<Stage::ApiKey>();
        break;
    case Stage::ApiKey:
        resetStage<Stage::Workspace>();
        break;
    case Stage::Workspace:
        resetStage<Stage::Project>();
        break;
    default:
        break;
    }
}

void SetupFlow::resetPreviousStage()
{
    switch (m_stage)
    {
    case Stage::ApiKey:
        resetStage<Stage::TimeService>();
        break;
    case Stage::Workspace:
        resetStage<Stage::ApiKey>();
        break;
    case Stage::Project:
        resetStage<Stage::Workspace>();
        break;
    case Stage::Done:
        resetStage<Stage::Project>();
        break;
    default:
        break;
    }
}

void SetupFlow::resetCurrentStage()
{
    switch (m_stage)
    {
    case Stage::TimeService:
        resetStage<Stage::TimeService>();
        break;
    case Stage::ApiKey:
        resetStage<Stage::ApiKey>();
        break;
    case Stage::Workspace:
        resetStage<Stage::Workspace>();
        break;
    case Stage::Project:
        resetStage<Stage::Project>();
        break;
    default:
        break;
    }
}

template<>
SetupFlow::Result SetupFlow::runStage<SetupFlow::Stage::TimeService>() const
{
    if (Settings::instance()->timeService().isEmpty())
    {
        logs::app()->trace("Initializing time service");
        bool ok{false};
        QString service = QInputDialog::getItem(nullptr,
                                                tr("Time service"),
                                                tr("Choose which time service to use:"),
                                                QStringList{} << QStringLiteral("Clockify") << QStringList("TimeCamp"),
                                                0,
                                                false,
                                                &ok);
        if (service == QStringLiteral("Clockify"))
            Settings::instance()->setTimeService(QStringLiteral("com.clockify"));
        else if (service == QStringLiteral("TimeCamp"))
        {
            Settings::instance()->setTimeService(QStringLiteral("com.timecamp"));
            Settings::instance()->setEventLoopInterval(15000);
        }

        return ok ? (service.isEmpty() ? SetupFlow::Result::Invalid : SetupFlow::Result::Valid) : SetupFlow::Result::Canceled;
    }

    return SetupFlow::Result::Valid;
}

template<>
SetupFlow::Result SetupFlow::runStage<SetupFlow::Stage::ApiKey>() const
{
    Result r = SetupFlow::Result::Valid;

    if (Settings::instance()->apiKey().isEmpty())
    {
        logs::app()->trace("Initializing API key");
        bool ok{false};
        QString newKey =
            QInputDialog::getText(nullptr, tr("API key"), tr("Enter your API key:"), QLineEdit::Normal, QString{}, &ok);
        if (ok)
        {
            Settings::instance()->setApiKey(newKey);
            if (m_parent->m_manager)
                m_parent->m_manager->setApiKey(newKey);
        }

        r = ok ? (newKey.isEmpty() ? SetupFlow::Result::Invalid : SetupFlow::Result::Valid) : SetupFlow::Result::Canceled;
    }

    if (r == SetupFlow::Result::Valid)
    {
        if (Settings::instance()->timeService() == QStringLiteral("com.clockify"))
            m_parent->initializeManager<ClockifyManager>();
        else if (Settings::instance()->timeService() == QStringLiteral("com.timecamp"))
            m_parent->initializeManager<TimeCampManager>();
        else
        {
            // since the service is invalid, we'll just select Clockify...
            Settings::instance()->setTimeService("com.clockify");
            m_parent->initializeManager<ClockifyManager>();
        }
    }

    return r;
}

template<>
SetupFlow::Result SetupFlow::runStage<SetupFlow::Stage::Workspace>() const
{
    Result r = SetupFlow::Result::Valid;

    if (Settings::instance()->workspaceId().isEmpty())
    {
        logs::app()->trace("Initializing workspace");
        auto workspaces = m_parent->m_manager->workspaces();
        QStringList names;
        for (const auto &w : workspaces)
            names << w.name();
        if (Settings::instance()->workspaceId().isEmpty())
        {
            bool ok{false};
            QString workspace = QInputDialog::getItem(
                nullptr, tr("Workspace"), tr("Choose which workspace to track time on:"), names, 0, false, &ok);

            if (ok)
                for (const auto &w : workspaces)
                    if (w.name() == workspace)
                    {
                        Settings::instance()->setWorkspaceId(w.id());
                        break;
                    }

            r = ok ? (Settings::instance()->workspaceId().isEmpty() ? SetupFlow::Result::Invalid : SetupFlow::Result::Valid) : SetupFlow::Result::Canceled;
        }
    }
    if (r == SetupFlow::Result::Valid)
        m_parent->m_manager->setWorkspaceId(Settings::instance()->workspaceId());

    return r;
}

template<>
SetupFlow::Result SetupFlow::runStage<SetupFlow::Stage::Project>() const
{
    auto validProjectSet = [] {
        return (Settings::instance()->useLastProject() || !Settings::instance()->projectId().isEmpty()) &&
                       (Settings::instance()->useSeparateBreakTime() ? !Settings::instance()->breakTimeId().isEmpty() :
                                                                       true);
    };

    if (!validProjectSet())
    {
        logs::app()->trace("Initializing project ID(s)");
        SettingsDialog d{m_parent->m_manager,
                         {{QStringLiteral("Clockify"), QStringLiteral("com.clockify")},
                          {QStringLiteral("TimeCamp"), QStringLiteral("com.timecamp")}}};
        d.switchToPage(SettingsDialog::Pages::ProjectPage);
        QMessageBox::information(
            nullptr, tr("Select a project"), tr("Please select a default project in the following dialog."));
        d.exec();
    }

    return validProjectSet() ? SetupFlow::Result::Valid : SetupFlow::Result::Invalid;
}

template<>
void SetupFlow::resetStage<SetupFlow::Stage::TimeService>() const
{
    Settings::instance()->setTimeService({});
}

template<>
void SetupFlow::resetStage<SetupFlow::Stage::ApiKey>() const
{
    Settings::instance()->setApiKey({});
}

template<>
void SetupFlow::resetStage<SetupFlow::Stage::Workspace>() const
{
    Settings::instance()->setWorkspaceId({});
}

template<>
void SetupFlow::resetStage<SetupFlow::Stage::Project>() const
{
    Settings::instance()->setProjectId({});
    Settings::instance()->setBreakTimeId({});
}
