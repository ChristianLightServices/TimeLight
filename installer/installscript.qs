// Uninstall stuff based on https://nheko.im/nheko-reborn/nheko/-/blob/master/deploy/installer/controlscript.qs.
// Everything else is probably my own random imaginings, with a bit of StackOverflow thrown in.

function silentUninstall()
{
    return installer.value("PleaseDeleteMeWithNaryAWordOfProtest") === "AbsolutelyGoRightAhead";
}

function Controller()
{
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    if (systemInfo.productType !== "windows")
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);

    if (silentUninstall() && installer.isUninstaller())
    {
        installer.setDefaultPageVisible(QInstaller.Introduction, false);
        installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
        installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
        installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
        installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);

        this.FinishedPageCallback = function()
        {
            gui.clickButton(buttons.FinishButton);
        }
    }
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    var page = gui.currentPageWidget();
    page.TargetDirectoryLineEdit.textChanged.connect(this, Controller.prototype.checkForExistingInstall);
    Controller.prototype.checkForExistingInstall(page.TargetDirectoryLineEdit.text);
}

Controller.prototype.checkForExistingInstall = function(text)
{
    let oldInstaller = text + "/maintenancetool.exe";
    if (text != "" && installer.fileExists(oldInstaller))
        if (QMessageBox.question("uninstallprevious.question",
                                 "Remove previous installation",
                                 "Do you want to remove the previous installation of TimeLight before installing this version?",
                                 QMessageBox.Yes | QMessageBox.No)
                === QMessageBox.Yes)
            installer.execute(oldInstaller, ["PleaseDeleteMeWithNaryAWordOfProtest=AbsolutelyGoRightAhead"]);
}

Controller.prototype.PerformInstallationPageCallback = function()
{
    let processName = (systemInfo.productType === "windows" ? "TimeLight.exe" : "TimeLight");
    if (installer.isUninstaller() && installer.isProcessRunning(processName))
        installer.killProcess(processName);
}

Controller.prototype.FinishedPageCallback = function()
{
    var page = gui.currentPageWidget();
    if (page != null)
    {
        if (systemInfo.productType === "windows" && !installer.isUninstaller())
            page.MessageLabel.setText("If you want TimeLight to automatically start when you start your computer, hold down the Windows key and press the R key. The Run dialog will appear. Type 'shell:startup' into the Run dialog (without the quotes) and press Enter. Windows Explorer will open. Now copy the TimeLight shortcut from your desktop into that Explorer window.\n\nClick Finish to exit the installer.");
    }
}
