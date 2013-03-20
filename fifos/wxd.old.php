#!/usr/bin/php -q
<?php
ini_set("date.timezone","Europe/Madrid");
// DEFAULT VAULES
$file="/usr/local/telescope/archive/logs/wxd.dat";
$delay=1;
$host="rebei";
$port=6666;
$echo=1;
// PROCESS ARGUMENTS
array_shift($argv);
$param=array_shift($argv);
while($param) {
	switch($param) {
		case "-f":
			$file=array_shift($argv);
			break;
		case "-d":
			$delay=array_shift($argv);
			break;
		case "-h":
			$host=array_shift($argv);
			break;
		case "-p":
			$port=array_shift($argv);
			break;
		case "-e":
			$echo=array_shift($argv);
			break;
		default:
			die("Unknown argument $param\n");
			break;
	}
	$param=array_shift($argv);
}
// MAIN LOOP, HAVE FUN
while(1) {
	// OPEN CONNECTION
	$fp=@fsockopen($host,$port);
	if($fp) {
		// READ DATA
		$rebei="";
		while(!feof($fp)) $rebei.=fgets($fp,128);
		fclose($fp);
		// CONVERT TO ARRAY
		$rebei=explode(" ",$rebei);
		foreach($rebei as $key=>$val) $rebei[$key]=explode(":",$val);
		// CHECK RESULTS
		if(isset($rebei[10][2])) {
			// PREPARE VARIABLES
			$windspeed=intval($rebei[7][2]);
			$winddir=intval($rebei[8][2]);
			$temperature=floatval($rebei[3][2]);
			$humidity=intval($rebei[6][2]);
			$pressure=floatval($rebei[9][2]);
			$rain=floatval($rebei[10][2]);
			// PREPARE TIMESTAMP
			$now=time();
			$month=date("m",$now);
			$day=date("d",$now);
			$year=date("Y",$now);
			$hour=date("H",$now);
			$minute=date("i",$now);
			$second=date("s",$now);
			$time=gregoriantojd($month,$day,$year)+(((($hour-12)*3600+$minute*60+$second)%86400)/86400);
			// DUMP OUTPUT
			$buffer="$time $windspeed $winddir $temperature $humidity $pressure $rain -----\n";
			file_put_contents($file,$buffer);
			if($echo) echo $buffer;
		} else {
			echo "Skiping invalid data from $host:$port\n";
		}
	} else {
		echo "Could not open socket to $host:$port\n";
	}
	sleep($delay);
}
?>
