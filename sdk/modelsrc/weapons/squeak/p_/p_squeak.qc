$modelname "p_squeak.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "pit2_ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869902 0 -4.140396 0 1.399666 0
$hbox 0 "Bip01 R Hand" 0 0 0 8.688218 2.985893 1.027838
$hbox 0 "Cylinder01" -4.77 -12.95 -7.59 4.95 8.04 21.4

$sequence "idle" {
	"..\anims\idle"
	fps 1
	loop
}
