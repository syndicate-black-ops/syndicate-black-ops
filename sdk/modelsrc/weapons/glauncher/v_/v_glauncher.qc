$modelname "v_glauncher.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "vindicobase"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "skin1.bmp" "skin2.bmp" }
	{ "skin1_lh.bmp" "skin2_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -3.15 -1.73 -1.56 12.99 1.34 1.72
$hbox 0 "Bip01 R Arm2" -3.01 -1.76 -2.18 12.79 1.31 1.29
$hbox 0 "Bip01 R Hand" -0.8 -0.4 -1.805943 4.388213 0.694793 1.73
$hbox 0 "Bone02" -6.06 -9.76 -2.64 4.05 5.55 1.1
$hbox 0 "Bone10" -0.250629 -2.208961 -1.79 2.62 0 0
$hbox 0 "Structure01" -3.13 -1.12 -2.08 2.82 11.14 1.92
$hbox 0 "Structure04" -4.83 -2.69 -0.19 1.52 2.1 2.21
$hbox 0 "Grenade1" -0.36 -0.65 -0.96 0.92 0.63 1.08
$hbox 0 "Grenade2" -0.04 -0.73 -1.1 1.27 0.58 0.94

$sequence "idle" {
	"..\anims\idle"
	fps 16
}
$sequence "idle2" {
	"..\anims\idle2"
	fps 16
}
$sequence "fire1" {
	"..\anims\fire1"
	fps 12
}
$sequence "fire2" {
	"..\anims\fire2"
	fps 12
}
$sequence "reload" {
	"..\anims\reload"
	fps 13
}
$sequence "deploy" {
	"..\anims\deploy"
	fps 16
}
