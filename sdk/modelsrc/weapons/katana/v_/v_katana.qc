$modelname "v_katana.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "troncref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.88 -1.95 -1.55 11.9 1.15 1.89
$hbox 0 "Bip01 L Hand" -0.35 -0.22 -1.6 4.23 1.02 1.63
$hbox 0 "Dummy28" 0 0 -1.207495 3.407557 7.328526 0
$hbox 0 "Dummy30" -0.64 -0.97 -0.33 5.14 32.639999 3
$hbox 0 "Dummy32" -7.86 -32.810001 -3.13 2.92 18.16 3.3
$hbox 0 "Bip01 R Arm2" -3.03 -2.11 -2.34 11.92 1.15 1.49
$hbox 0 "Bip01 R Hand" -0.34 -0.71 -1.64 4.53938 1.497721 1.74
$hbox 0 "Dummy22" 0 0 -1.207493 5.42564 24.857679 0
$hbox 0 "Dummy24" -1 -1.59 -2.27 5.49 31.9 0.51
$hbox 0 "Dummy23" -7.46 -28.74 -3.86 4.09 22.139999 1.74

$sequence "idle1" {
	"..\anims\idle1"
	fps 110
	loop
}
$sequence "draw" {
	"..\anims\draw"
	fps 35
}
$sequence "holster" {
	"..\anims\holster"
	fps 24
}
$sequence "attack1R" {
	"..\anims\attack1r"
	fps 20
}
$sequence "attack2R" {
	"..\anims\attack2r"
	fps 41
}
$sequence "attack1L" {
	"..\anims\attack1l"
	fps 41
}
$sequence "attack2L" {
	"..\anims\attack2l"
	fps 41
}
