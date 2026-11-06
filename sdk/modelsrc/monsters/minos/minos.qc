$modelname "minos.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "minosref"
}

$flags 0

$eyeposition -20 0 220

$attachment 0 "Bone07" 80 0 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Pelvis" -38.02 -12.2 -24.709013 7.290237 35.040001 23.954718
$hbox 6 "Bip01 L Leg" 0 -15.54 -17.67 59.160583 30.690001 33.77
$hbox 6 "Bip01 L Leg1" 0 -5.86 -18.030001 55.299835 23.969999 14.57
$hbox 6 "Bip01 L Foot" 0 -5.72 -20.32 20.440001 21.469999 6.81
$hbox 7 "Bip01 R Leg" 0 -15.54 -22.09 58.617821 29.530001 14.49
$hbox 7 "Bip01 R Leg1" 0 -6.58 -20.33 78.980003 23.65 14.03
$hbox 3 "Bip01 Spine" -16.01 -10.16 -25.57 10.90618 29.76 27.84
$hbox 2 "Bip01 Spine1" -0.27 -14.78 -37.68 17.247728 32.77 32.369999
$hbox 2 "Bip01 Spine2" 0 -16.700001 -38.389999 16.721003 32.630001 40.66
$hbox 2 "Bip01 Spine3" 0 -18.09 -0.001496 17.530001 16.34 36.509998
$hbox 2 "Bip01 Neck" -10.100582 -13.72 -13.37 7.927209 12.38 9.23
$hbox 4 "Bip01 L Arm" 0 -15.95 -4.7 19.602497 15.45 11.76
$hbox 4 "Bip01 L Arm1" 0 -19.23 -14.21 41.24017 13.55 12.94
$hbox 4 "Bip01 L Arm2" 0 -18.15 -11.97 65.870003 10.06 13.51
$hbox 4 "Bip01 L Hand" 0 -14.42 -10.13 32.330002 4.53 6.9
$hbox 5 "Bone05" 0 0 0 22.450325 1.167274 7.998343
$hbox 5 "Bone07" -6.52 -6.34 -8.06 79.550003 8.75 7.16
$hbox 5 "Bip01 R Arm" -42.610001 -18 -11.04 37.91 15.76 34.52
$hbox 5 "Bip01 R Arm1" 0 -19.08 -13.51 47.639999 6.96 14.37
$hbox 5 "Bip01 R Arm2" 0 -19.75 -13.86 49.672134 8.48 9.61
$hbox 5 "Bip01 R Hand" 0 -21.469999 -6.55 26.26 0 9.69
$hbox 1 "Bip01 Head" -10.22 -24.879999 -32.869999 31.059999 32.27 33.349998
$hbox 2 "Bone03" 0 -6.55 -10.01 2.16 0 0
$hbox 2 "Bone01" -17.93 -25.129999 -14.59 3 0.33 7.46

$sequence "combat_idle" {
	"..\anims\combat_idle"
	ACT_COMBAT_IDLE 1
	fps 5
	loop
}
$sequence "die" {
	"..\anims\die"
	ACT_DIESIMPLE 1
	{ event 5 0 }
	fps 30
}
$sequence "fight1" {
	"..\anims\fight1"
	ACT_MELEE_ATTACK1 1
	{ event 2 0 }
	fps 25
	loop
}
$sequence "fight2" {
	"..\anims\fight2"
	ACT_MELEE_ATTACK1 2
	{ event 2 0 }
	fps 25
	loop
}
$sequence "fight3" {
	"..\anims\fight3"
	ACT_MELEE_ATTACK1 3
	{ event 2 0 }
	fps 20
	loop
}
$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 1
	fps 10
	loop
}
$sequence "shoot" {
	"..\anims\shoot_blend01"
	"..\anims\shoot_blend02"
	"..\anims\shoot_blend03"
	ACT_RANGE_ATTACK1 1
	blend XR -260 260
	{ event 1 1 }
	{ event 5001 1 "93" }
	fps 150
	loop
}
$sequence "walk" {
	"..\anims\walk"
	ACT_WALK 1
	{ event 3 20 }
	{ event 4 41 }
	fps 25
	loop
	LX
}
$sequence "run" {
	"..\anims\run"
	ACT_WALK 1
	{ event 3 20 }
	{ event 4 41 }
	fps 25
	loop
	LX
}
