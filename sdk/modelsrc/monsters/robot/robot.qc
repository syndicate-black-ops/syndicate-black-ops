$modelname "robot.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "roboref"
}

$flags 0

$eyeposition 20 0 220

$attachment 0 "Arme" 80 0 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 3 "Bip01 Pelvis" -4.17 -7.76 -12.48 9.857937 7.51 12.26
$hbox 3 "Bip01 Spine" -9.857923 -12.56 -16.030001 17.867756 10.85 15.99
$hbox 2 "Bip01 Spine1" -4.46 -15.71 -17.879999 18.280001 11.65 17.950001
$hbox 1 "Bip01 Head" -3.08 -12.15 -9.05 17.42 9.59 9.68
$hbox 2 "Bip01 L Clavicle" -3.4 -15.85 -9.43 21.799999 18.219999 9.21
$hbox 4 "Bip01 L UpperArm" 0 -8.13 -5.03 25.77 6.82 5.88
$hbox 4 "Bip01 L Forearm" 0 -5.24 -3.88 23.440142 5.53 6.2
$hbox 4 "Bip01 L Hand" 0 -1.81 -9.54 10.16 2.02 3.7
$hbox 2 "Bip01 R Clavicle" -1.75 -15.98 -10.96 22.66 18.15 9.34
$hbox 5 "Bip01 R UpperArm" 0 -9.02 -5.87 24.607265 5.9 4.57
$hbox 5 "Bip01 R Forearm" 0 -6.71 -6.03 22.373281 4.88 3.98
$hbox 5 "Bip01 R Hand" -0.93 -4.11 -6.54 6.99 5.12 13.86
$hbox 6 "Bip01 L Thigh" -3.08 -10.43 -6.95 27.790001 7.39 7.98
$hbox 6 "Bip01 L Calf" 0 -5.8 -4.08 23.559999 4.63 4.42
$hbox 6 "Bip01 L Foot" 0 -8.51 -7.86 10.1 19.690001 8.05
$hbox 7 "Bip01 R Thigh" -3.21 -10.22 -7.69 27.66 7.55 7.26
$hbox 7 "Bip01 R Calf" 0 -5.62 -3.99 23.389999 4.82 4.52
$hbox 7 "Bip01 R Foot" 0 -8.33 -7.66 9.93 19.870001 8.24

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
$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 1
	fps 5
	loop
}
$sequence "shoot" {
	"..\anims\shoot_blend01"
	"..\anims\shoot_blend02"
	"..\anims\shoot_blend03"
	ACT_RANGE_ATTACK1 1
	blend XR -260 260
	{ event 1 0 }
	{ event 5001 0 "93" }
	fps 20
	loop
}
$sequence "walk" {
	"..\anims\walk"
	ACT_WALK 1
	{ event 3 22 }
	{ event 4 43 }
	fps 20
	loop
	LX
}
$sequence "run" {
	"..\anims\run"
	ACT_WALK 1
	{ event 3 22 }
	{ event 4 43 }
	fps 20
	loop
	LX
}
