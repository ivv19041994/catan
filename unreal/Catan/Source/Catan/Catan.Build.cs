using UnrealBuildTool;
using System.IO;

public class Catan : ModuleRules
{
    public Catan(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent",
            "UMG", "Slate", "SlateCore", "ApplicationCore", "CommonUI", "OnlineSubsystem",
            "OnlineSubsystemUtils", "Sockets", "Networking"
        });

        string CoreSource = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../../src"));
        PublicIncludePaths.Add(CoreSource);
    }
}
