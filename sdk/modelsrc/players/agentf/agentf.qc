$modelname "agentf.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "ho"
}

$flags 0

$attachment 0 "Bip01 R Hand" 28 2.8 7
$attachment 1 "Bip01 R Hand" 22 2.2 5.5
$attachment 2 "Bip01 R Hand" 35 3.5 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 4 "Bip01 Pelvis" -35.049999 -3.88 -3.713254 2.4 7.46 3.713254
$hbox 6 "Bip01 L Leg" 0 -3.89 -3.77 18.045723 4.19 0.000001
$hbox 6 "Bip01 L Leg1" 0 -2.95 -2.82 18.139999 0.02 0.000001
$hbox 6 "Bip01 L Foot" -0.54 0 -3.03 4.62 7.84 0.28
$hbox 6 "Bip01 L Toe02" 0 -1.01 -2.47 2.73 0.03 0
$hbox 6 "Bip01 R Leg" 0 -3.89 0 18.045723 4.19 3.26
$hbox 6 "Bip01 R Leg1" 0 -2.91 -0.000002 18.059999 0.06 2.31
$hbox 6 "Bip01 R Foot" -0.53 0 -0.79 4.63 7.94 2.52
$hbox 6 "Bip01 R Toe02" 0 -1.02 -0.22 2.84 0.02 1.95
$hbox 3 "Bip01 Spine" 0 -3.83 -6.08 5.309683 4.22 6.59
$hbox 2 "Bip01 Spine1" 0 -2.42 -4.65 4.826789 3.43 5.17
$hbox 2 "Bip01 Spine2" 0 -3.85 -4.56 5.792768 5.83 5.07
$hbox 2 "Bip01 Spine3" -0.07 -5.24 -4.99 6.662323 6.1 5.51
$hbox 0 "Bip01 Neck" -3.112025 -1.31 -4.140396 2.046984 1.54 4.341911
$hbox 1 "Bip01 Head" -2.32 -6.21 -3.53 8.09 2.53 4.17
$hbox 4 "Bip01 L Arm" 0 -2.46 -3.15 4.827231 3.19 2.72
$hbox 4 "Bip01 L Arm1" 0 -1.59 -1.24 11.139764 2.04 2.1
$hbox 4 "Bip01 L Arm2" 0 -0.71 -0.67 10.314596 1.62 1.26
$hbox 4 "Bip01 L Hand" 0 -0.62 -2.17 2.975833 0.68 0.91
$hbox 4 "Bip01 R Arm" 0 -2.45 -2.45 4.972048 3.27 1.58
$hbox 4 "Bip01 R Arm1" 0 -1.59 -1.9 11.585353 2.03 1.43
$hbox 4 "Bip01 R Arm2" 0 -0.72 -1.15 10.127059 1.62 0.78
$hbox 4 "Bip01 R Hand" -0.18 -0.54 -0.84 2.683748 0.83 2.38

$controller 0 "Bip01 Spine" XR -30 30
$controller 1 "Bip01 Spine1" XR -30 30
$controller 2 "Bip01 Spine2" XR -30 30
$controller 3 "Bip01 Spine3" XR -30 30

$sequence "look_idle" {
	"..\anims\look_idle"
	ACT_IDLE 2
	fps 14
	loop
}
$sequence "idle" {
	"..\anims\idle"
	ACT_IDLE 1
	fps 14
	loop
}
$sequence "deep_idle" {
	"..\anims\deep_idle"
	ACT_IDLE 4
	fps 12
	loop
}
$sequence "run2" {
	"..\anims\run2"
	ACT_RUN 1
	fps 40
	loop
	LX
}
$sequence "walk2handed" {
	"..\anims\walk2handed"
	ACT_WALK 1
	fps 26
	loop
	LX
}
$sequence "2handshoot" {
	"..\anims\2handshoot"
	ACT_RANGE_ATTACK1 1
	fps 20
}
$sequence "crawl" {
	"..\anims\crawl"
	ACT_CROUCH 1
	fps 22
	loop
	LX
}
$sequence "crouch_idle" {
	"..\anims\crouch_idle"
	ACT_CROUCHIDLE 1
	fps 12
	loop
}
$sequence "jump" {
	"..\anims\jump"
	ACT_HOP 1
	fps 30
}
$sequence "long_jump" {
	"..\anims\long_jump"
	ACT_LEAP 1
	fps 24
	LX
}
$sequence "swim" {
	"..\anims\swim"
	ACT_SWIM 1
	fps 14
	loop
}
$sequence "treadwater" {
	"..\anims\treadwater"
	ACT_HOVER 1
	fps 14
	loop
}
$sequence "run" {
	"..\anims\run"
	fps 31
	loop
	LX
}
$sequence "walk" {
	"..\anims\walk"
	fps 31
	loop
	LX
}
$sequence "aim_2" {
	"..\anims\aim_2_blend01"
	"..\anims\aim_2_blend02"
	blend XR -45 45
	fps 14
	loop
}
$sequence "shoot_2" {
	"..\anims\shoot_2_blend01"
	"..\anims\shoot_2_blend02"
	blend XR -45 45
	fps 14
	loop
}
$sequence "aim_1" {
	"..\anims\aim_1_blend01"
	"..\anims\aim_1_blend02"
	blend XR -45 45
	fps 14
	loop
}
$sequence "shoot_1" {
	"..\anims\shoot_1_blend01"
	"..\anims\shoot_1_blend02"
	blend XR -45 45
	fps 14
	loop
}
$sequence "die_simple" {
	"..\anims\die_simple"
	ACT_DIESIMPLE 1
	{ event 2001 9 }
	fps 22
}
$sequence "die_backwards1" {
	"..\anims\die_backwards1"
	ACT_DIEBACKWARD 4
	{ event 2001 13 }
	fps 26
}
$sequence "die_backwards" {
	"..\anims\die_backwards"
	ACT_DIEBACKWARD 1
	{ event 2001 15 }
	fps 30
}
$sequence "die_forwards" {
	"..\anims\die_forwards"
	ACT_DIEFORWARD 1
	{ event 2001 8 }
	fps 26
}
$sequence "headshot" {
	"..\anims\headshot"
	ACT_DIE_HEADSHOT 4
	{ event 2001 22 }
	fps 24
}
$sequence "die_spin" {
	"..\anims\die_spin"
	ACT_DIE_HEADSHOT 1
	{ event 2001 16 }
	fps 30
}
$sequence "gutshot" {
	"..\anims\gutshot"
	ACT_DIE_GUTSHOT 1
	{ event 2001 22 }
	fps 26
}
$sequence "ref_aim_crowbar" {
	"..\anims\ref_aim_crowbar_blend01"
	"..\anims\ref_aim_crowbar_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_crowbar" {
	"..\anims\ref_shoot_crowbar_blend01"
	"..\anims\ref_shoot_crowbar_blend02"
	blend XR -45 45
	fps 24
}
$sequence "crouch_aim_crowbar" {
	"..\anims\crouch_aim_crowbar_blend01"
	"..\anims\crouch_aim_crowbar_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_crowbar" {
	"..\anims\crouch_shoot_crowbar_blend01"
	"..\anims\crouch_shoot_crowbar_blend02"
	blend XR -45 45
	fps 24
}
$sequence "ref_aim_trip" {
	"..\anims\ref_aim_trip_blend01"
	"..\anims\ref_aim_trip_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_trip" {
	"..\anims\ref_shoot_trip_blend01"
	"..\anims\ref_shoot_trip_blend02"
	blend XR -45 45
	fps 18
}
$sequence "crouch_aim_trip" {
	"..\anims\crouch_aim_trip_blend01"
	"..\anims\crouch_aim_trip_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_trip" {
	"..\anims\crouch_shoot_trip_blend01"
	"..\anims\crouch_shoot_trip_blend02"
	blend XR -45 45
	fps 16
}
$sequence "ref_aim_onehanded" {
	"..\anims\ref_aim_onehanded_blend01"
	"..\anims\ref_aim_onehanded_blend02"
	blend XR -50 35
	fps 12
	loop
}
$sequence "ref_shoot_onehanded" {
	"..\anims\ref_shoot_onehanded_blend01"
	"..\anims\ref_shoot_onehanded_blend02"
	blend XR -50 35
	{ event 5011 0 "21" }
	fps 16
}
$sequence "crouch_aim_onehanded" {
	"..\anims\crouch_aim_onehanded_blend01"
	"..\anims\crouch_aim_onehanded_blend02"
	blend XR -50 35
	fps 12
	loop
}
$sequence "crouch_shoot_onehanded" {
	"..\anims\crouch_shoot_onehanded_blend01"
	"..\anims\crouch_shoot_onehanded_blend02"
	blend XR -50 35
	{ event 5011 0 "21" }
	fps 16
}
$sequence "ref_aim_python" {
	"..\anims\ref_aim_python_blend01"
	"..\anims\ref_aim_python_blend02"
	blend XR -50 35
	fps 12
	loop
}
$sequence "ref_shoot_python" {
	"..\anims\ref_shoot_python_blend01"
	"..\anims\ref_shoot_python_blend02"
	blend XR -50 35
	{ event 5011 0 "31" }
	fps 16
}
$sequence "crouch_aim_python" {
	"..\anims\crouch_aim_python_blend01"
	"..\anims\crouch_aim_python_blend02"
	blend XR -50 35
	fps 12
	loop
}
$sequence "crouch_shoot_python" {
	"..\anims\crouch_shoot_python_blend01"
	"..\anims\crouch_shoot_python_blend02"
	blend XR -50 35
	{ event 5011 0 "31" }
	fps 16
}
$sequence "ref_aim_shotgun" {
	"..\anims\ref_aim_shotgun_blend01"
	"..\anims\ref_aim_shotgun_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_shotgun" {
	"..\anims\ref_shoot_shotgun_blend01"
	"..\anims\ref_shoot_shotgun_blend02"
	blend XR -45 45
	{ event 5001 0 "51" }
	fps 15
}
$sequence "crouch_aim_shotgun" {
	"..\anims\crouch_aim_shotgun_blend01"
	"..\anims\crouch_aim_shotgun_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_shotgun" {
	"..\anims\crouch_shoot_shotgun_blend01"
	"..\anims\crouch_shoot_shotgun_blend02"
	blend XR -45 45
	{ event 5001 0 "51" }
	fps 15
}
$sequence "ref_aim_gauss" {
	"..\anims\ref_aim_gauss_blend01"
	"..\anims\ref_aim_gauss_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_gauss" {
	"..\anims\ref_shoot_gauss_blend01"
	"..\anims\ref_shoot_gauss_blend02"
	blend XR -45 45
	fps 15
}
$sequence "crouch_aim_gauss" {
	"..\anims\crouch_aim_gauss_blend01"
	"..\anims\crouch_aim_gauss_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_gauss" {
	"..\anims\crouch_shoot_gauss_blend01"
	"..\anims\crouch_shoot_gauss_blend02"
	blend XR -45 45
	fps 15
}
$sequence "ref_aim_mp5" {
	"..\anims\ref_aim_mp5_blend01"
	"..\anims\ref_aim_mp5_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_mp5" {
	"..\anims\ref_shoot_mp5_blend01"
	"..\anims\ref_shoot_mp5_blend02"
	blend XR -45 45
	{ event 5021 0 "40" }
	fps 20
	loop
}
$sequence "crouch_aim_mp5" {
	"..\anims\crouch_aim_mp5_blend01"
	"..\anims\crouch_aim_mp5_blend02"
	blend XR -30 40
	fps 12
	loop
}
$sequence "crouch_shoot_mp5" {
	"..\anims\crouch_shoot_mp5_blend01"
	"..\anims\crouch_shoot_mp5_blend02"
	blend XR -30 40
	{ event 5021 0 "40" }
	fps 20
	loop
}
$sequence "ref_aim_rpg" {
	"..\anims\ref_aim_rpg_blend01"
	"..\anims\ref_aim_rpg_blend02"
	blend XR -45 40
	fps 12
	loop
}
$sequence "ref_shoot_rpg" {
	"..\anims\ref_shoot_rpg_blend01"
	"..\anims\ref_shoot_rpg_blend02"
	blend XR -45 40
	fps 18
}
$sequence "crouch_aim_rpg" {
	"..\anims\crouch_aim_rpg_blend01"
	"..\anims\crouch_aim_rpg_blend02"
	blend XR -45 40
	fps 12
	loop
}
$sequence "crouch_shoot_rpg" {
	"..\anims\crouch_shoot_rpg_blend01"
	"..\anims\crouch_shoot_rpg_blend02"
	blend XR -45 40
	fps 18
}
$sequence "ref_aim_egon" {
	"..\anims\ref_aim_egon_blend01"
	"..\anims\ref_aim_egon_blend02"
	blend XR -45 35
	fps 12
	loop
}
$sequence "ref_shoot_egon" {
	"..\anims\ref_shoot_egon_blend01"
	"..\anims\ref_shoot_egon_blend02"
	blend XR -45 35
	fps 18
	loop
}
$sequence "crouch_aim_egon" {
	"..\anims\crouch_aim_egon_blend01"
	"..\anims\crouch_aim_egon_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_egon" {
	"..\anims\crouch_shoot_egon_blend01"
	"..\anims\crouch_shoot_egon_blend02"
	blend XR -45 45
	fps 18
	loop
}
$sequence "ref_aim_squeak" {
	"..\anims\ref_aim_squeak_blend01"
	"..\anims\ref_aim_squeak_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_squeak" {
	"..\anims\ref_shoot_squeak_blend01"
	"..\anims\ref_shoot_squeak_blend02"
	blend XR -45 45
	fps 14
}
$sequence "crouch_aim_squeak" {
	"..\anims\crouch_aim_squeak_blend01"
	"..\anims\crouch_aim_squeak_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_squeak" {
	"..\anims\crouch_shoot_squeak_blend01"
	"..\anims\crouch_shoot_squeak_blend02"
	blend XR -45 45
	fps 14
}
$sequence "ref_aim_hive" {
	"..\anims\ref_aim_hive_blend01"
	"..\anims\ref_aim_hive_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_hive" {
	"..\anims\ref_shoot_hive_blend01"
	"..\anims\ref_shoot_hive_blend02"
	blend XR -45 45
	fps 15
}
$sequence "crouch_aim_hive" {
	"..\anims\crouch_aim_hive_blend01"
	"..\anims\crouch_aim_hive_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_hive" {
	"..\anims\crouch_shoot_hive_blend01"
	"..\anims\crouch_shoot_hive_blend02"
	blend XR -45 45
	fps 15
}
$sequence "ref_aim_bow" {
	"..\anims\ref_aim_bow_blend01"
	"..\anims\ref_aim_bow_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_bow" {
	"..\anims\ref_shoot_bow_blend01"
	"..\anims\ref_shoot_bow_blend02"
	blend XR -45 45
	fps 16
}
$sequence "crouch_aim_bow" {
	"..\anims\crouch_aim_bow_blend01"
	"..\anims\crouch_aim_bow_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_bow" {
	"..\anims\crouch_shoot_bow_blend01"
	"..\anims\crouch_shoot_bow_blend02"
	blend XR -45 45
	fps 16
}
$sequence "deadback" {
	"..\anims\deadback"
	fps 10
}
$sequence "deadsitting" {
	"..\anims\deadsitting"
	fps 10
}
$sequence "deadstomach" {
	"..\anims\deadstomach"
	fps 10
}
$sequence "deadtable" {
	"..\anims\deadtable"
	fps 10
}
$sequence "ref_aim_epee" {
	"..\anims\ref_aim_epee_blend01"
	"..\anims\ref_aim_epee_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "ref_shoot_epee" {
	"..\anims\ref_shoot_epee_blend01"
	"..\anims\ref_shoot_epee_blend02"
	blend XR -45 45
	fps 18
}
$sequence "crouch_aim_epee" {
	"..\anims\crouch_aim_epee_blend01"
	"..\anims\crouch_aim_epee_blend02"
	blend XR -45 45
	fps 12
	loop
}
$sequence "crouch_shoot_epee" {
	"..\anims\crouch_shoot_epee_blend01"
	"..\anims\crouch_shoot_epee_blend02"
	blend XR -45 45
	fps 18
}
