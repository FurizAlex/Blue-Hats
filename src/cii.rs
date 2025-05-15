use clap::{cmd, Args, Commands};
use crate::{installer, manifest};

pub fn run()
{
	let matches = cmd!().subcommand(Commands::new("install").arg(Args::new("package").required(true))).get_matches();

	match matches.subcommand()
	{
		Some(("install"), sub_m) =>
		{
			let package = sub_m.get_one::<String>("package").unwrap();
			installer::install(package);
		}
		_ => println!("Unknown command.")
	}
}