﻿using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class AppConstants : SingletonMonoBehaviour<AppConstants>
{
    public string PoolFilename = "dice_list.json";
    public string DataSetFilename = "dice_data.json";
    public float ScanTimeout = 3.0f;
    public float DiceRotationSpeedAvg = 10.0f;
    public float DiceRotationSpeedVar = 1.0f;
}

