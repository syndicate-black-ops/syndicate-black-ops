$modelname "w_weaponbox.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "backpack_template1"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Backpack1" -10.22 -15.89 -11.23 12.59 12.35 11.03

$sequence "idle" {
	"..\anims\idle"
	fps 10
	loop
}
