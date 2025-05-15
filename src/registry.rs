pub fn getPackageURL(name: &str, version: &str) -> String
{
	format!("https://packages.bluehats.com/{}/{}.zip", name, version);
}