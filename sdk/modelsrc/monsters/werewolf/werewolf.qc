$modelname "werewolf.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 3 "Bip01 Pelvis" -3.6 -7.59 -9.53 4.46 9.67 9.84
$hbox 2 "Bip01 Spine1" -4.38 -7.3 -7.36 12.99 10.14 7.67
$hbox 2 "Bip01 Spine2" 0 -0.003362 0 4.123013 2.12 10.25
$hbox 2 "Bip01 Spine3" -6.61 -9.93 -10.81 4.217781 11.41 11.12
$hbox 2 "Bip01 Neck" 0 0 -3.69 8.61 4.24 4
$hbox 1 "Bip01 Head" 0 -7.81 -5.91 17.23 13 6.22
$hbox 1 "Bone01" 0 0 -1.34 11.13 3.51 1.55
$hbox 1 "Bone05" 0 0 -1.12 10.67 7.05 1.34
$hbox 1 "Bone03" 0 0 -1.13 14.01 2.17 1.34
$hbox 4 "Bip01 L UpperArm" 0 -5.26 -5.2 20.137974 4.18 3.74
$hbox 4 "Bip01 L ForeArm" 0 -5.82 -3.56 12.684426 3.32 3.4
$hbox 4 "Bip01 L Hand" 0 -2.2 -4.13 9.25 2.27 1
$hbox 5 "Bip01 R Clavicle" 0 0 -2.79 7.55 2.33 0
$hbox 5 "Bip01 R UpperArm" 0 -5.85 -3.68 18.790001 3.76 5.24
$hbox 5 "Bip01 R ForeArm" 0 -6.08 -3.3 12.744323 2.74 3.64
$hbox 5 "Bip01 R Hand" 0 -2.08 -1.07 11.06 2.38 4.06
$hbox 2 "Bip01 L Thigh" 0 -7.98 -5.92 22.9 7.48 5.6
$hbox 2 "Bip01 L Calf" 0 -6.04 -4.47 18.878098 6.01 5.36
$hbox 6 "Bip01 L Foot" -1.34 -0.39 -2.39 5.01 15.8 3.75
$hbox 2 "Bip01 R Thigh" 0 -7.02 -5.9 22.85 7.48 5.61
$hbox 2 "Bip01 R Calf" 0 -6.05 -5.62 18.878099 6 4.17
$hbox 7 "Bip01 R Foot" -1.4 -0.39 -4.05 4.95 15.8 2.09

$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 1
	fps 13
	loop
}
$sequence "idle1" {
	"..\anims\idle1_2"
	ACT_IDLE 2
	fps 13
	loop
}
$sequence "idle1" {
	"..\anims\idle1_3"
	ACT_IDLE 8
	fps 13
	loop
}
$sequence "idle1" {
	"..\anims\idle1_4"
	ACT_CROUCH 1
	fps 23
}
$sequence "walk1" {
	"..\anims\walk1"
	ACT_WALK 1
	fps 30
	loop
	LX
}
$sequence "walk2" {
	"..\anims\walk2"
	ACT_WALK 1
	fps 30
	loop
	LX
}
$sequence "run1" {
	"..\anims\run1"
	ACT_RUN 1
	fps 30
	loop
	LX
}
$sequence "right" {
	"..\anims\right"
	ACT_TURN_LEFT 1
	fps 230
}
$sequence "idle1" {
	"..\anims\idle1_5"
	ACT_TURN_RIGHT 1
	fps 230
}
$sequence "attack1" {
	"..\anims\attack1"
	ACT_MELEE_ATTACK1 4
	{ event 1 7 }
	{ event 1 13 }
	{ event 1 15 }
	fps 33
}
$sequence "zapattack1" {
	"..\anims\zapattack1"
	ACT_RANGE_ATTACK1 1
	{ event 3 0 "{" }
	{ event 3 4 }
	{ event 3 10 }
	{ event 3 15 }
	{ event 4 24 }
	{ event 5 29 }
	fps 33
}
$sequence "idle1" {
	"..\anims\idle1_6_blend01"
	"..\anims\idle1_6_blend02"
	fps 0
}
$sequence "idle1" {
	"..\anims\idle1_7"
	ACT_FLINCH_LEFTARM 1
	fps 200
}
$sequence "idle1" {
	"..\anims\idle1_8"
	ACT_FLINCH_RIGHTARM 1
	fps 200
}
$sequence "idle1" {
	"..\anims\idle1_9"
	ACT_FLINCH_LEFTLEG 1
	fps 200
}
$sequence "idle1" {
	"..\anims\idle1_10"
	ACT_FLINCH_RIGHTLEG 1
	fps 200
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot"
	ACT_DIE_HEADSHOT 1
	fps 60
	X Y
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot_2"
	ACT_DIESIMPLE 1
	fps 60
	X Y
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot_3"
	ACT_DIEBACKWARD 1
	fps 60
	X Y
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot_4"
	ACT_DIEFORWARD 1
	fps 60
	X Y
}
$sequence "idle1" {
	"..\anims\idle1_11"
	ACT_RELOAD 1
	fps 200
}
$sequence "idle1" {
	"..\anims\idle1_12"
	ACT_RELOAD 1
	fps 200
}
$sequence "jump" {
	"..\anims\jump"
	fps 23
	X
}
$sequence "walk1" {
	"..\anims\walk1_2"
	fps 23
	loop
	LX LZ
}
$sequence "idle1" {
	"..\anims\idle1_13"
	fps 200
}
$sequence "walk1" {
	"..\anims\walk1_3"
	fps 200
}
$sequence "walk1" {
	"..\anims\walk1_4"
	fps 200
}
$sequence "walk1" {
	"..\anims\walk1_5"
	fps 200
}
$sequence "idle1" {
	"..\anims\idle1_14"
	fps 200
	loop
}
$sequence "idle1" {
	"..\anims\idle1_15"
	fps 200
	loop
}
