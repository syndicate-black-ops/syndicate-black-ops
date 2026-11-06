$modelname "v_rpg.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "rpg_rocket"
}

$bodygroup "body"
{
	studio "refait"
}

$bodygroup "body"
{
	studio "rpg_hand"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "v_rpg-null.bmp" }
	{ "v_rpg-null_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "rocket_master" -1.86 -8.62 -1.65 1.93 4.14 2.14
$hbox 0 "Bip01 R Hand" 0 0 0 8.085783 1.563943 7.994959
$hbox 0 "Dummy22" -3.07 -10.94 -5.99 5.37 10.85 2.29
$hbox 0 "Bip01 L Arm2" 0 -2.03 -1.39 11.32 2.41 2.98
$hbox 0 "Bip01 L Hand" 0 -0.93 -1.85 4.18 1.04 1.78

$sequence "idle" {
	"..\anims\idle"
	fps 15
	loop
}
$sequence "fidget" {
	"..\anims\fidget"
	fps 15
	loop
}
$sequence "reload" {
	"..\anims\reload"
	fps 30
}
$sequence "fire" {
	"..\anims\fire"
	fps 30
}
$sequence "holster1" {
	"..\anims\holster1"
	fps 30
}
$sequence "draw1" {
	"..\anims\draw1"
	fps 30
}
$sequence "holster2" {
	"..\anims\holster2"
	fps 30
}
$sequence "draw2" {
	"..\anims\draw2"
	fps 30
}
$sequence "idle2" {
	"..\anims\idle2"
	fps 15
	loop
}
$sequence "fidget2" {
	"..\anims\fidget2"
	fps 15
	loop
}
