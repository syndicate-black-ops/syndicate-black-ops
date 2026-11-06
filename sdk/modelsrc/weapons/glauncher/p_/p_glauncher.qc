$modelname "p_glauncher.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_glauncher"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" 0 -0.29 -3.3 29.24 4.57 7.57

$sequence "idle" {
	"..\anims\idle"
	fps 30
}
