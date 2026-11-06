$modelname "v_ben.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "ladenref"
}

$flags 0

$attachment 0 "Bone07" 0 0 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.18 -2.18 -2.29 11.46 2.74 2.08
$hbox 0 "Bip01 L Hand" 0 -0.000905 -1.78 4.79 1.96 1.71
$hbox 0 "Bip01 R Arm2" -1.89 -1.89 -2.4 11.62 1.9 1.76
$hbox 0 "Bip01 R Hand" -3.682889 -0.4 -1.634995 4.51 1.692147 2.87
$hbox 0 "Bone03" 0 -0.000001 -5.475352 6.378041 43.688148 0
$hbox 0 "Bone07" -42.189999 -5.47 -1.39 0.09 2.44 1.07
$hbox 0 "Bone17" 0 -2.72 -1.23 17.4 0 0.99
$hbox 0 "Bone14" 0 0 -0.65 1.12 2.07 0.58
$hbox 0 "Bone20" 0 -38.689999 -0.34 6.73 0 0.73

$sequence "idle" {
	"..\anims\idle"
	fps 20
	loop
}
$sequence "shoot" {
	"..\anims\shoot"
	{ event 5001 0 "41" }
	fps 30
}
$sequence "seagullkiller" {
	"..\anims\seagullkiller"
	{ event 5001 0 "41" }
	fps 30
}
$sequence "insert" {
	"..\anims\insert"
	fps 65
}
$sequence "after_reload" {
	"..\anims\after_reload"
	fps 30
}
$sequence "start_reload" {
	"..\anims\start_reload"
	fps 30
}
$sequence "draw" {
	"..\anims\draw"
	fps 30
}
