using UnrealBuildTool;
using System.IO;

public class LiteRTLMUnreal : ModuleRules
{
	public LiteRTLMUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Projects", "Json" });

		// ThirdParty Path
		string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "ThirdParty/LiteRtLm"));
		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "Include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string BinariesPath = Path.Combine(ThirdPartyPath, "Binaries/Win64");
			
			// Runtime Dependencies for DLLs
			foreach (string Dll in Directory.EnumerateFiles(BinariesPath, "*.dll"))
			{
				RuntimeDependencies.Add("$(BinaryOutputDir)/" + Path.GetFileName(Dll), Dll);
			}
		}
	}
}
