$modelname "islave.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "islave_template_model-1"
}

$flags 0

$eyeposition 0 0 43

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 3 "Bip01 Pelvis" -5.14 -10.67 -8.77 6.06 6.58 8.12
$hbox 6 "Bip01 L Leg" 0 -6.16 -5.14 20.459999 6.28 2.25
$hbox 6 "Bip01 L Leg1" 0 -5.14 -3.94 13.411352 3.04 1.92
$hbox 6 "Bip01 L Foot" 0 -4.48 -2.17 8.87 6.67565 4.36
$hbox 6 "Bone13" -2.82 0 -2.66 8.21 6.98 0
$hbox 7 "Bip01 R Leg" -0.93 -6.16 -2.08 20.459999 6.32 5.15
$hbox 7 "Bip01 R Leg1" 0 -5.15 -1.97 13.411351 3.04 4.01
$hbox 7 "Bip01 R Foot" 0 -2.72 -4.43 7.05 1.62 2.21
$hbox 7 "Bone19" -8.78 -9.48 -2.85 2.27 7 0
$hbox 3 "Bip01 Spine" 0 -8.44 -5.2 6.15089 5.69 5.37
$hbox 2 "Bip01 Spine1" 0 -17.32 -9.12 12.71 6.78 9.22
$hbox 2 "Bip01 Spine2" 0 -16.02 -9.18 14.37 7.23552 9.22
$hbox 2 "Bip01 Neck" -10.85 -5.58 -4.87 3.408091 6.56921 5.01
$hbox 4 "Bip01 L Arm" 0 -3.36 -4.26 9.88 10.74 4.09
$hbox 4 "Bip01 L Arm1" 0 -5.7 -2.93 22.314196 3.08 3.17
$hbox 4 "Bip01 L Arm2" -2.62 -9.19 -3.28 13.466397 0 3.8
$hbox 4 "Bip01 L Hand" -1.98 -3.8 -4.6 6.690087 0 1.047432
$hbox 4 "Bip01 L Finger0" -4.22 -1.79 -5.01 7.78 8.7 0
$hbox 4 "Bip01 L Finger1" -7.78 -0.08 -3.09 4.06 11.78 0.32
$hbox 5 "Bip01 R Arm" 0 -3.42 -4 10.3 10.77 7.6
$hbox 5 "Bip01 R Arm1" 0 -0.86 -2.33 22.314196 7.51 3.47
$hbox 5 "Bip01 R Arm2" -4.02 -7.26 -2.75 13.466398 2.54 4.39
$hbox 5 "Bip01 R Hand" -2.99 -0.17 -5.1 11.64 11.93 3.33
$hbox 1 "Bip01 Head" -20.58 -22.440001 -8.31 4.66 13.39 8.06
$hbox 4 "Dummy04" -3.96 -5.38 -4.46 3.27 0 4.37
$hbox 4 "Bone06" -5.8 -7.46 -1.36 0.76 0.68 0.94
$hbox 4 "Bone09" -5.2 -7.59 -2.38 1.79 0.69 1.77
$hbox 4 "Bone30" -0.64 -5.5 -1.01 1.1 0.7 2.65

$controller 0 "Bip01 Neck1" YR 60 -60

$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 1
	fps 23
	loop
}
$sequence "idle2" {
	"..\anims\idle2"
	ACT_IDLE 2
	fps 23
	loop
}
$sequence "idle3" {
	"..\anims\idle3"
	ACT_IDLE 8
	fps 23
	loop
}
$sequence "crouch" {
	"..\anims\crouch"
	ACT_CROUCH 1
	fps 23
}
$sequence "walk1" {
	"..\anims\walk1"
	ACT_WALK 1
	fps 23
	loop
	LX
}
$sequence "walk2" {
	"..\anims\walk2"
	ACT_WALK 1
	fps 23
	loop
	LX
}
$sequence "run1" {
	"..\anims\run1"
	ACT_RUN 1
	fps 38
	loop
	LX
}
$sequence "right" {
	"..\anims\right"
	ACT_TURN_LEFT 1
	fps 23
}
$sequence "left" {
	"..\anims\left"
	ACT_TURN_RIGHT 1
	fps 23
}
$sequence "attack1" {
	"..\anims\attack1"
	ACT_MELEE_ATTACK1 4
	{ event 1 7 }
	{ event 1 13 }
	{ event 1 21 }
	fps 33
}
$sequence "zapattack1" {
	"..\anims\zapattack1"
	ACT_RANGE_ATTACK1 1
	{ event 3 0 }
	{ event 3 4 }
	{ event 3 10 }
	{ event 3 15 }
	{ event 4 24 }
	{ event 5 29 }
	fps 23
}
$sequence "flinch2" {
	"..\anims\flinch2"
	ACT_SMALL_FLINCH 1
	fps 23
}
$sequence "laflinch" {
	"..\anims\laflinch"
	ACT_FLINCH_LEFTARM 1
	fps 23
}
$sequence "raflinch" {
	"..\anims\raflinch"
	ACT_FLINCH_RIGHTARM 1
	fps 23
}
$sequence "llflinch" {
	"..\anims\llflinch"
	ACT_FLINCH_LEFTLEG 1
	fps 23
}
$sequence "rlflinch" {
	"..\anims\rlflinch"
	ACT_FLINCH_RIGHTLEG 1
	fps 23
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot"
	ACT_DIE_HEADSHOT 1
	fps 30
	X Y
}
$sequence "diesimple" {
	"..\anims\diesimple"
	ACT_DIESIMPLE 1
	fps 30
	X Y
}
$sequence "diebackward" {
	"..\anims\diebackward"
	ACT_DIEBACKWARD 1
	fps 30
	X Y
}
$sequence "dieforward" {
	"..\anims\dieforward"
	ACT_DIEFORWARD 1
	fps 60
	X Y
}
$sequence "collar1" {
	"..\anims\collar1"
	ACT_RELOAD 1
	fps 27
}
$sequence "collar2" {
	"..\anims\collar2"
	ACT_RELOAD 1
	fps 27
}
$sequence "jump" {
	"..\anims\jump"
	fps 23
	X
}
$sequence "stairsup" {
	"..\anims\stairsup"
	fps 23
	loop
	LX LZ
}
$sequence "pushup" {
	"..\anims\pushup"
	fps 30
}
$sequence "grab" {
	"..\anims\grab"
	fps 30
}
$sequence "updown" {
	"..\anims\updown"
	fps 30
}
$sequence "downup" {
	"..\anims\downup"
	fps 30
}
$sequence "jibber" {
	"..\anims\jibber"
	{ event 1008 19 "aslave/slv_word5.wav" }
	{ event 1008 52 "aslave/slv_word7.wav" }
	fps 45
	loop
}
$sequence "jabber" {
	"..\anims\jabber"
	{ event 1008 9 "aslave/slv_word3.wav" }
	{ event 1008 39 "aslave/slv_word4.wav" }
	fps 45
	loop
}
