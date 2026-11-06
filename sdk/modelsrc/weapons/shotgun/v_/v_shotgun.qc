$modelname "v_shotgun.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "sparef"
}

$flags 0

$attachment 0 "Bone11" -3 -5 1.5

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -17.799999 -6.02 -2.19 11.82 1.15 5.54
$hbox 0 "Bip01 R Arm2" -17.26 -6.42 -6.1 11.88 1.38 2.23
$hbox 0 "Bip01 R Hand" -0.35 -0.4 -1.80293 4.328004 0.62 1.74
$hbox 0 "Bone02" -7.56 -22.68 -6.78 6.31 16.91 9.35
$hbox 0 "Bone04" 0 0 -4.846752 3.487528 15.544702 0
$hbox 0 "Bone08" -2.83 -18.469999 0 1.320814 0 6.35
$hbox 0 "Bone10" -3.06 -9.05427 -1.48 0.85468 2.95 4.203465
$hbox 0 "Bone12" -4.14 -5.04 -0.98 1.320814 0 1.47
$hbox 0 "Bone14" -6.67 0 -2.58 0 6.8 0.14

$sequence "spaidle" {
	"..\anims\spaidle"
	fps 15
	loop
}
$sequence "spashoot1" {
	"..\anims\spashoot1"
	{ event 5001 0 "31" }
	fps 20
}
$sequence "spashoot2" {
	"..\anims\spashoot2"
	{ event 5001 0 "41" }
	{ event 5001 1 "41" }
	{ event 5001 2 "41" }
	fps 30
}
$sequence "spareload" {
	"..\anims\spareload"
	fps 25
}
$sequence "pump" {
	"..\anims\pump"
	fps 13
}
$sequence "start_reload" {
	"..\anims\start_reload"
	fps 25
}
$sequence "spadraw" {
	"..\anims\spadraw"
	fps 25
}
$sequence "spaholster" {
	"..\anims\spaholster"
	fps 30
}
$sequence "idle4" {
	"..\anims\idle4"
	fps 9
	loop
}
$sequence "deepidle" {
	"..\anims\deepidle"
	fps 12
	loop
}
