$modelname "p_rifle.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_plasma"
}

$flags 0

$attachment 0 "Fire" 0 0 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869902 0 -4.140396 0 1.399666 0
$hbox 0 "Bip01 R Finger1" 0 0 0 40.637905 13.602333 7.36535
$hbox 0 "Fire" -59.52 -3.25 -11.03 5.52 5.45 7.17

$sequence "idle" {
	"..\anims\idle"
	fps 25
	loop
}
