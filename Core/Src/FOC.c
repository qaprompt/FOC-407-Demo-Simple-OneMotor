#include "FOC.h"
#include "arm_math.h"

#include "stdio.h"
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"

#include "Svpwm.h"
#define FOC_SQRT_3 1.73205f
#define FOC_ANGLE_TO_RADIN 0.01745f

volatile uint32_t gFoc_TimeCNT;

#include "Motor1SPI1.h"
// FOC框架
#include "Foc.h"
// 电机使能引脚
// 电机1SVPWM生成
#include "Svpwm.h"
// 电机1电流采集
#include "Motor1ADC1PWM.h"

#define AD_TO_CURRENT 0.00032

#define M1_OUTMAX 12.0f * 0.577f
#define M1_KP 0.018f
#define M1_KI 0.018f
#define M1_KD 0.0f

/*************************************************************
** Function name:       GetMotor1PreCurrent
** Descriptions:        获取电机1 3相电流值
** Input parameters:    None
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void GetMotor1PreCurrent(float *ua, float *ub, float *uc)
{
    *ua = GetMotor1ADC1PhaseXValue(0) * AD_TO_CURRENT * 1.5;
    *ub = GetMotor1ADC1PhaseXValue(1) * AD_TO_CURRENT * 1.5;
    *uc = GetMotor1ADC1PhaseXValue(2) * AD_TO_CURRENT * 1.5;
}

float Motor1TLE5012BReadAngel(void)
{
    return ReadTLE5012BAngle(11.832636f); // angleOffect
}

// 声明FOC对象
FOC_Struct gMotor1FOC = {
    .isEnable = 0,
    .polePairs = 7.0f, // xpolePairs,
    .tarid = 0.0,
    .tariq = 0.0,
    .mAngle = 0.0,
    .angle = 0.0,
    .radian = 0.0,
    .iNum = 3, // xiNum,
    .ia = 0.0,
    .ib = 0.0,
    .ic = 0.0,
    .iα = 0.0,
    .iβ = 0.0,
    .iq = 0.0,
    .id = 0.0,
    .iαSVPWM = 0.0,
    .iβSVPWM = 0.0,
    .idPID = {0},
    .iqPID = {0},
    .SetEnable = Motor1SetEnable,
    .GetEncoderAngle = Motor1TLE5012BReadAngel,
    .GetSVPWMSector = Motor1GetSVPWMSector,
    .GetPreCurrent = GetMotor1PreCurrent,
    .SvpwmGenerate = Motor1SvpwmGenerate,
};
/*************************************************************
** Function name:       Motor1FOCConfig_Init
** Descriptions:        电机一FOC初始化
** Input parameters:    None
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void Motor1FOCConfig_Init(void)
{
    // 设置电机1参数
    SetFocEnable(&gMotor1FOC, 1);
    SetCurrentPIDParams(&gMotor1FOC, M1_KP, M1_KI, M1_KD, M1_OUTMAX);
    // 设置目标电流
    SetCurrentPIDTar(&gMotor1FOC, 0, 0);
}

/*************************************************************
** Function name:       Motor1FOCConfig_Printf
** Descriptions:        电机一FOC打印函数
** Input parameters:    None
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void Motor1FOCConfig_Printf(void)
{
    FOCPrintf(&gMotor1FOC);
}

/*************************************************************
** Function name:       Motor1FocControl
** Descriptions:        电机一FOC控制函数
** Input parameters:    None
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void Motor1FocControl(void)
{
    FocContorl(&gMotor1FOC);
}
/*************************************************************
** Function name:       Motor1SetTarIDIQ
** Descriptions:        设置电机1目标电流
** Input parameters:    id：d轴电流单位A
**						iq: q轴电流单位A
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void Motor1SetTarIDIQ(float id, float iq)
{
    SetCurrentPIDTar(&gMotor1FOC, id, iq);
}
/*************************************************************
** Function name:       GetMotor1Angle
** Descriptions:        获取电机1机械角度
** Input parameters:    None
** Output parameters:   None
** Returned value:      电机2机械角度
** Remarks:             None
*************************************************************/
float Motor1GetAngle(void)
{
    return GetFocAngle(&gMotor1FOC);
}
/*************************************************************
** Function name:       GetElectricalAngle
** Descriptions:        获取电气角度
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void GetElectricalAngle(PFOC_Struct pFOC)
{
    pFOC->mAngle = pFOC->GetEncoderAngle();
    pFOC->angle = pFOC->mAngle * pFOC->polePairs;
    pFOC->radian = pFOC->angle * FOC_ANGLE_TO_RADIN;
}
/*************************************************************
** Function name:       CurrentReconstruction
** Descriptions:        电流重构
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void CurrentReconstruction(PFOC_Struct pFOC)
{
    pFOC->GetPreCurrent(&pFOC->ia, &pFOC->ib, &pFOC->ic);
    if (pFOC->iNum < 3)
    {
        return;
    }
    switch (pFOC->GetSVPWMSector())
    {
    case 1:
        pFOC->ia = 0.0f - pFOC->ib - pFOC->ic;
        break;
    case 2:
        pFOC->ib = 0.0f - pFOC->ia - pFOC->ic;
        break;
    case 3:
        pFOC->ib = 0.0f - pFOC->ia - pFOC->ic;
        break;
    case 4:
        pFOC->ic = 0.0f - pFOC->ia - pFOC->ib;
        break;
    case 5:
        pFOC->ic = 0.0f - pFOC->ia - pFOC->ib;
        break;
    case 6:
        pFOC->ia = 0.0f - pFOC->ib - pFOC->ic;
        break;
    default:
        break;
    }
}
/*************************************************************
** Function name:       ClarkeTransform
** Descriptions:        Clarke正变换
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void ClarkeTransform(PFOC_Struct pFOC)
{
    pFOC->iα = pFOC->ia;
    pFOC->iβ = (pFOC->ia + 2.0f * pFOC->ib) / FOC_SQRT_3;
}
/*************************************************************
** Function name:       ParkTransform
** Descriptions:        Park正变换
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void ParkTransform(PFOC_Struct pFOC)
{
    pFOC->id = pFOC->iα * arm_cos_f32(pFOC->radian) + pFOC->iβ * arm_sin_f32(pFOC->radian);
    pFOC->iq = -pFOC->iα * arm_sin_f32(pFOC->radian) + pFOC->iβ * arm_cos_f32(pFOC->radian);
}

/*************************************************************
** Function name:       ParkAntiTransform
** Descriptions:        Park反变换
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void ParkAntiTransform(PFOC_Struct pFOC)
{
    pFOC->iαSVPWM = pFOC->idPID.out * arm_cos_f32(pFOC->radian) - pFOC->iqPID.out * arm_sin_f32(pFOC->radian);
    pFOC->iβSVPWM = pFOC->idPID.out * arm_sin_f32(pFOC->radian) + pFOC->iqPID.out * arm_cos_f32(pFOC->radian);
}

/*************************************************************
** Function name:       CurrentPIControlID
** Descriptions:        D轴电流闭环
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void CurrentPIControlID(PFOC_Struct pFOC)
{
    // 获取实际值
    pFOC->idPID.pre = pFOC->id;
    // 获取目标值
    pFOC->idPID.tar = pFOC->tarid;
    // 计算偏差
    pFOC->idPID.bias = pFOC->idPID.tar - pFOC->idPID.pre;
    // 计算PID输出值
    pFOC->idPID.out += pFOC->idPID.kp * (pFOC->idPID.bias - pFOC->idPID.lastBias) + pFOC->idPID.ki * pFOC->idPID.bias;
    // 保存偏差
    pFOC->idPID.lastBias = pFOC->idPID.bias;

    if (pFOC->idPID.out > fabs(pFOC->idPID.outMax))
    {
        pFOC->idPID.out = fabs(pFOC->idPID.outMax);
    }

    if (pFOC->idPID.out < -fabs(pFOC->idPID.outMax))
    {
        pFOC->idPID.out = -fabs(pFOC->idPID.outMax);
    }
}
/*************************************************************
** Function name:       CurrentPIControlIQ
** Descriptions:        Q轴电流闭环
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
static void CurrentPIControlIQ(PFOC_Struct pFOC)
{
    // 获取实际值
    pFOC->iqPID.pre = pFOC->iq;
    // 获取目标值
    pFOC->iqPID.tar = pFOC->tariq;
    // 计算偏差
    pFOC->iqPID.bias = pFOC->iqPID.tar - pFOC->iqPID.pre;
    // 计算PID输出值
    pFOC->iqPID.out += pFOC->iqPID.kp * (pFOC->iqPID.bias - pFOC->iqPID.lastBias) + pFOC->iqPID.ki * pFOC->iqPID.bias;
    // 保存偏差
    pFOC->iqPID.lastBias = pFOC->iqPID.bias;

    if (pFOC->iqPID.out > fabs(pFOC->iqPID.outMax))
    {
        pFOC->iqPID.out = fabs(pFOC->iqPID.outMax);
    }

    if (pFOC->iqPID.out < -fabs(pFOC->iqPID.outMax))
    {
        pFOC->iqPID.out = -fabs(pFOC->iqPID.outMax);
    }
}

/*************************************************************
** Function name:       FocContorl
** Descriptions:        FOC控制流程
** Input parameters:    pFOC:结构体指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void FocContorl(PFOC_Struct pFOC)
{
    // 0.获取电气角度
    GetElectricalAngle(pFOC);
    // 1计算实际电流值
    // 1.0电流重构
    CurrentReconstruction(pFOC);
    // 1.1Clarke变换
    ClarkeTransform(pFOC);
    // 1.2Park变换
    ParkTransform(pFOC);
    // 2.做PID闭环
    CurrentPIControlID(pFOC);
    CurrentPIControlIQ(pFOC);
    //	pFOC->idPID.out = 0.0;
    //	pFOC->iqPID.out = 0.0;
    // 3.计算输出值iα i贝塔
    ParkAntiTransform(pFOC);
    // 4.输出SVPWM
    pFOC->SvpwmGenerate(pFOC->iαSVPWM, pFOC->iβSVPWM);
    return;
}
/*************************************************************
** Function name:       SetCurrentPIDTar
** Descriptions:        设置D轴和Q轴的目标值
** Input parameters:    pFOC：FOC对象指针
**                      tarid：D轴目标电流
**                      tariq：Q轴目标电流
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void SetCurrentPIDTar(PFOC_Struct pFOC, float tarid, float tariq)
{
    pFOC->tarid = tarid;
    pFOC->tariq = tariq;
}
/*************************************************************
** Function name:       SetCurrentPIDParams
** Descriptions:        设置电流环参数
** Input parameters:    pFOC：FOC对象指针
**                      kp:比例
**                      ki:积分
**                      kd:微分
**                      outMax:输出限幅
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void SetCurrentPIDParams(PFOC_Struct pFOC, float kp, float ki, float kd, float outMax)
{
    pFOC->idPID.kp = kp;
    pFOC->idPID.ki = ki;
    pFOC->idPID.kd = kd;
    pFOC->idPID.outMax = outMax;

    pFOC->iqPID.kp = kp;
    pFOC->iqPID.ki = ki;
    pFOC->iqPID.kd = kd;
    pFOC->iqPID.outMax = outMax;
}
/*************************************************************
** Function name:       SetFocEnable
** Descriptions:        设置FOC使能
** Input parameters:    pFOC：FOC对象指针
**                      isEnable:是否使能
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void SetFocEnable(PFOC_Struct pFOC, uint8_t isEnable)
{
    pFOC->isEnable = isEnable;
    pFOC->SetEnable(pFOC->isEnable);
}
/*************************************************************
** Function name:       FOCPrintf
** Descriptions:        FOC打印信息
** Input parameters:    None
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
void FOCPrintf(PFOC_Struct pFOC)
{
    printf("1:%f\r\n", pFOC->ia);
    printf("2:%f\r\n", pFOC->ib);
    printf("3:%f\r\n", pFOC->ic);
    printf("4:%f\r\n", pFOC->angle);

    printf("5:%f\r\n", pFOC->id);
    printf("6:%f\r\n", pFOC->iq);
    printf("7:%f\r\n", pFOC->idPID.tar);
    printf("8:%f\r\n", pFOC->iqPID.tar);
}

/*************************************************************
** Function name:       GetFocAngle
** Descriptions:        获取FOC机械角度
** Input parameters:    pFOC：FOC对象指针
** Output parameters:   None
** Returned value:      None
** Remarks:             None
*************************************************************/
float GetFocAngle(PFOC_Struct pFOC)
{
    return pFOC->mAngle;
}
