$modelname "v_canon.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "terrorref"
}

$flags 0

$attachment 0 "Dummy28" 0 0 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -3.74 -1.64 -1.47 11.69 1.29 1.85
$hbox 0 "Bip01 L Hand" 0 -0.28 -1.66 4.4 0.94 1.74
$hbox 0 "Bip01 R Arm2" -4.79 -1.78 -2.29 11.6 1.39 1.42
$hbox 0 "Bip01 R Hand" -1.32 -0.55 -1.94 4.51 0.75 1.93
$hbox 0 "Dummy26" -2.85 -2.79 -1.56 8.72 0.61 1.61
$hbox 0 "Dummy24" -0.02 -0.85 -1.34 2.34 0.19 1.41
$hbox 0 "Dummy22" -6.55 -6.19 -1.34 2.62 0.26 2.6

$sequence "fireboth" {
	"..\anims\fireboth"
	{ event 5001 0 "41" }
	{ event 5011 0 "41" }
	fps 30
}
$sequence "fireL" {
	"..\anims\firel"
	{ event 5001 0 "31" }
	fps 30
}
$sequence "fireR" {
	"..\anims\firer"
	{ event 5001 0 "31" }
	fps 30
}
$sequence "reload" {
	"..\anims\reload"
	fps 28
}
$sequence "deploy" {
	"..\anims\deploy"
	fps 15
}
$sequence "putaway" {
	"..\anims\putaway"
	fps 15
}
$sequence "idle" {
	"..\anims\idle"
	fps 15
	loop
}
