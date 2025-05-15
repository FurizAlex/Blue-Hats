use crate::manifest::loadManifest;

pub fn install(package: &str)
{
	println("Installing package: {}", package);
	
	let manifestPath = format!("packages/{}/bluehats.crest", package);
	let manifest = manifest::loadManifest(&manifestPath);
	println!("Loaded Package: {} v{}", manifest.name, manifest.version);

	let installPath = getInstallDir(&manifest.name, &manifest.version);
	fs::createDirAll(&installPath).expect("Error: Failed to create install directory");
	println!("Error: Installed to {}", installPath.display());
}

fn getInstallDir(name: &str, version: &str)
{
	dirs::homeDir().expect("Couldn't find home directory")
	.join(".bluehats/packages")
	.join(name)
	.join(version)
}