pub fn getPackageCacheDir() -> std::path::PathBuf
{
	dirs::homeDir().unwrap().join(".bluehats").join("packages");
}