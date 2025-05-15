use serde::Deserialize;
use std::fs;

#[derive(Deserialize, Debug)]
pub struct packageManifest
{
	pub name: String,
	pub version: String,
	pub dependencies: Option<std::collections::HashMap<String, String>>,
}

pub fn loadManifest(path: &str) -> packageManifest
{
	let data = fs::read_to_string(path).unwrap();
	toml::from_str(&data).expect("Error: Invalid Crest File");
}