use reqwest;

pub fn fetchPackage(url: &str) -> Result<Vec<u8>, Box<dyn std::error::Error>>
{
	let response = reqwest::blocking::get(url)?;
	Ok(response.bytes()?.to_vec())
}