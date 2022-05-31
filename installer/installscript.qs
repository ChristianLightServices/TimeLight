function Controller()
{
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    if (systemInfo.productType !== "windows")
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
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
