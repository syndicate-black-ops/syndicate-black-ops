$modelname "p_tripmine.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "reference_tripmine"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" 0 0 0 4.278921 3.312595 1.946941
$hbox 0 "Box01" -4.08 -1.35 -2.72 4.1 2.66 0.7

$sequence "idle" {
	"..\anims\idle"
	fps 30
}
