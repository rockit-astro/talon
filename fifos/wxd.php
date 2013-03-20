#!/usr/bin/php -q
<?php
ini_set("date.timezone","Europe/Madrid");
// DEFAULT VAULES
$file="/usr/local/telescope/archive/logs/wxd.dat";
$delay=1;
$host="rebei.oadm.cat";
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
		case "-e":
			$echo=array_shift($argv);
			break;
		default:
			die("Unknown argument $param\n");
			break;
	}
	$param=array_shift($argv);
}
// FUNCTIONS
function snmp($oid) {
	global $host;
	$result=@snmpget($host,"public",$oid);
	$format=strtok($result,":");
	$output=trim(strtok(""));
	if($format=="STRING") $output=substr($output,1,-1);
	return $output;
}
// MAIN LOOP, HAVE FUN
while(1) {
	// PREPARE VARIABLES
	$time=floatval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.4"));
	$windspeed=intval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.27")*3.6+0.5);
	$winddir=intval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.31"));
	$temperature=floatval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.11"));
	$humidity=intval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.23"));
	$pressure=floatval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.35"));
	$rain=floatval(snmp(".1.3.6.1.4.1.2021.50.1.4.1.2.5.100.97.118.105.115.39"));
	// DUMP OUTPUT
	$buffer="$time $windspeed $winddir $temperature $humidity $pressure $rain -----\n";
	file_put_contents($file,$buffer);
	if($echo) echo $buffer;
	sleep($delay);
}
?>
