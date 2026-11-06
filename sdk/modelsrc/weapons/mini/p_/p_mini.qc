$modelname "p_mini.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_gatleen_ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -17.540001 -6.43 -2.74 52.009998 11.27 21.559999

$sequence "standthere" {
	"..\anims\standthere"
	fps 30
}
