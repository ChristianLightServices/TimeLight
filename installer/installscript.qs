function Controller()
{
}

function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.kernelType === "winnt")
        component.addOperation("CreateShortcut", "@TargetDir@/bin/TimeLight.exe", "@StartMenuDir@/Timelight");
}
