#include "com_math_anglevectors.h"

#include <cmath>
#include "q_shared.h"

void __cdecl AngleVectors(const float *angles, float *forward, float *right, float *up)
{
#ifdef KISAK_SP
    const float toRadians = 0.01745329238474369f;
    float yaw = angles[1] * toRadians;
    float cy = cos((double)yaw);
    float sy = sin((double)yaw);
    float pitch = angles[0] * toRadians;
    float cp = cos((double)pitch);
    float sp = sin((double)pitch);
    if (forward)
    {
        forward[1] = cp * sy;
        forward[0] = cp * cy;
        forward[2] = -sp;
    }
    if (right || up)
    {
        float roll = angles[2] * toRadians;
        float cr = cos((double)roll);
        float sr = sin((double)roll);
        if (right)
        {
            right[0] = cr * sy - sr * sp * cy;
            right[1] = cr * cy * -1.0f - sr * sp * sy;
            right[2] = sr * cp * -1.0f;
        }
        if (up)
        {
            up[0] = cr * sp * cy + sr * sy;
            up[1] = cr * sp * sy - sr * cy;
            up[2] = cr * cp;
        }
    }
#else
    float cy; // [esp+18h] [ebp-1Ch]
    float angle; // [esp+1Ch] [ebp-18h]
    float anglea; // [esp+1Ch] [ebp-18h]
    float angleb; // [esp+1Ch] [ebp-18h]
    float sr; // [esp+20h] [ebp-14h]
    float v9; // [esp+24h] [ebp-10h]
    float cr; // [esp+28h] [ebp-Ch]
    float cp; // [esp+2Ch] [ebp-8h]
    float sy; // [esp+30h] [ebp-4h]

    const double toRadians = 0.017453292;
    angle = angles[1] * toRadians;
    cy = cos(angle);
    sy = sin(angle);
    anglea = *angles * toRadians;
    cp = cos(anglea);
    v9 = sin(anglea);
    if ( forward )
    {
        *forward = cp * cy;
        forward[1] = cp * sy;
        //*((unsigned int *)forward + 2) = LODWORD(v9) ^ _mask__NegFloat_;
        forward[2] = -v9;
    }
    if ( right || up )
    {
        angleb = angles[2] * toRadians;
        cr = cos(angleb);
        sr = sin(angleb);
        if ( right )
        {
            //right[0] = (float)((float)((float)(-1.0 * sr) * v9) * cy) + (float)((float)(-1.0 * cr) * COERCE_FLOAT(LODWORD(sy) ^ _mask__NegFloat_));
            right[0] = (float)((float)((float)(-1.0 * sr) * v9) * cy) + (float)((float)(-1.0 * cr) * -sy);
            right[1] = (float)((float)((float)(-1.0 * sr) * v9) * sy) + (float)((float)(-1.0 * cr) * cy);
            right[2] = (float)(-1.0 * sr) * cp;
        }
        if ( up )
        {
            //up[0] = (float)((float)(cr * v9) * cy) + (float)(COERCE_FLOAT(LODWORD(sr) ^ _mask__NegFloat_) * COERCE_FLOAT(LODWORD(sy) ^ _mask__NegFloat_));
            up[0] = (float)((float)(cr * v9) * cy) + (float)(-sr * -sy);
            //up[1] = (float)((float)(cr * v9) * sy) + (float)(COERCE_FLOAT(LODWORD(sr) ^ _mask__NegFloat_) * cy);
            up[1] = (float)((float)(cr * v9) * sy) + (float)(-sr * cy);
            up[2] = cr * cp;
        }
    }
#endif
}

void __cdecl AnglesToAxis(const float *angles, float axis[3][3])
{
    float cy; // [esp+18h] [ebp-1Ch]
    float angle; // [esp+1Ch] [ebp-18h]
    float anglea; // [esp+1Ch] [ebp-18h]
    float angleb; // [esp+1Ch] [ebp-18h]
    float sr; // [esp+20h] [ebp-14h]
    float v7; // [esp+24h] [ebp-10h]
    float cr; // [esp+28h] [ebp-Ch]
    float cp; // [esp+2Ch] [ebp-8h]
    float sy; // [esp+30h] [ebp-4h]

#ifdef KISAK_SP
    const float toRadians = 0.01745329238474369f;
#else
    const double toRadians = 0.01745329238474369;
#endif
    angle = angles[1] * toRadians;
#ifdef KISAK_SP
    cy = cos((double)angle);
    sy = sin((double)angle);
#else
    cy = cos(angle);
    sy = sin(angle);
#endif
    anglea = angles[0] * toRadians;
#ifdef KISAK_SP
    cp = cos((double)anglea);
    v7 = sin((double)anglea);
#else
    cp = cos(anglea);
    v7 = sin(anglea);
#endif

    axis[0][0] = cp * cy;
    axis[0][1] = cp * sy;
    axis[0][2] = -v7;
    angleb = angles[2] * toRadians;
#ifdef KISAK_SP
    cr = cos((double)angleb);
    sr = sin((double)angleb);
#else
    cr = cos(angleb);
    sr = sin(angleb);
#endif
#ifdef KISAK_SP
    axis[1][0] = sr * v7 * cy - cr * sy;
#else
    axis[1][0] = sr * v7 * cy + -sy * cr;
#endif
    axis[1][1] = sr * v7 * sy + cr * cy;
    axis[1][2] = sr * cp;
#ifdef KISAK_SP
    axis[2][0] = cr * v7 * cy + sr * sy;
    axis[2][1] = cr * v7 * sy - sr * cy;
#else
    axis[2][0] = cr * v7 * cy + -sr * -sy;
    axis[2][1] = cr * v7 * sy + -sr * cy;
#endif
    axis[2][2] = cr * cp;
}

void __cdecl AnglesToQuat(const float *angles, float *quat)
{
    float axis[3][3]; // [esp+0h] [ebp-24h] BYREF

    AnglesToAxis(angles, axis);
    AxisToQuat(axis, quat);
}

void __cdecl QuatToAxis(const float *quat, float axis[3][3])
{
#ifdef KISAK_SP
    const float w = quat[3];
    const float y = quat[1];
    const float x = quat[0];
    const float z = quat[2];
    const volatile float yy = y * y;
    const volatile float ww = w * w;
    const volatile float zz = z * z;
    const volatile float sumWZ = ww + zz;
    const volatile float sumWZY = sumWZ + yy;
    const volatile float xx = x * x;
    const volatile float magSqr = sumWZY + xx;
    iassert(magSqr > 0.0f);
    const volatile float scale = 2.0f / magSqr;
    const volatile float xxa = scale * xx;
    const volatile float yya = scale * yy;
    const volatile float zza = scale * zz;
    const volatile float scaledX = x * scale;
    const volatile float xz = z * scaledX;
    const volatile float xw = w * scaledX;
    const volatile float xy = y * scaledX;
    const volatile float scaledY = y * scale;
    const volatile float yz = z * scaledY;
    const volatile float yw = w * scaledY;
    const volatile float productZW = w * z;
    const volatile float zw = productZW * scale;
    const volatile float diagonal0 = zza + yya;
    axis[0][0] = 1.0f - diagonal0;
    axis[0][1] = zw + xy;
    axis[0][2] = xz - yw;
    const volatile float diagonal1 = zza + xxa;
    axis[1][1] = 1.0f - diagonal1;
    const float xzPlusYW = yw + xz;
    axis[1][0] = xy - zw;
    axis[1][2] = yz + xw;
    axis[2][0] = xzPlusYW;
    axis[2][1] = yz - xw;
    const volatile float diagonal2 = yya + xxa;
    axis[2][2] = 1.0f - diagonal2;
#else
    float yy; // [esp+0h] [ebp-38h]
    float yya; // [esp+0h] [ebp-38h]
    float ww; // [esp+4h] [ebp-34h]
    float xy; // [esp+8h] [ebp-30h]
    float zz; // [esp+Ch] [ebp-2Ch]
    float zza; // [esp+Ch] [ebp-2Ch]
    float zw; // [esp+10h] [ebp-28h]
    float scaledX; // [esp+14h] [ebp-24h]
    float scaledY; // [esp+18h] [ebp-20h]
    float yw; // [esp+1Ch] [ebp-1Ch]
    float xz; // [esp+20h] [ebp-18h]
    float yz; // [esp+24h] [ebp-14h]
    float xx; // [esp+28h] [ebp-10h]
    float xxa; // [esp+28h] [ebp-10h]
    float xw; // [esp+2Ch] [ebp-Ch]
    float scale; // [esp+30h] [ebp-8h]
    float magSqr; // [esp+34h] [ebp-4h]

    xx = *quat * *quat;
    yy = quat[1] * quat[1];
    zz = quat[2] * quat[2];
    ww = quat[3] * quat[3];
    magSqr = xx + yy + zz + ww;
    iassert(magSqr > 0.0f);
    scale = 2.0 / magSqr;
    xxa = xx * scale;
    yya = yy * scale;
    zza = zz * scale;
    scaledX = scale * *quat;
    xy = scaledX * quat[1];
    xz = scaledX * quat[2];
    xw = scaledX * quat[3];
    scaledY = scale * quat[1];
    yz = scaledY * quat[2];
    yw = scaledY * quat[3];
    zw = scale * quat[2] * quat[3];

    (axis)[0][0] = 1.0 - (yya + zza);
    (axis)[0][1] = xy + zw;
    (axis)[0][2] = xz - yw;
    (axis)[1][0] = xy - zw;
    (axis)[1][1] = 1.0 - (xxa + zza);
    (axis)[1][2] = yz + xw;
    (axis)[2][0] = xz + yw;
    (axis)[2][1] = yz - xw;
    (axis)[2][2] = 1.0 - (xxa + yya);
#endif
}

void __cdecl AxisToSignedAngles(const float (*axis)[3], float *angles)
{
    float v2; // [esp+0h] [ebp-38h]
    float rad; // [esp+18h] [ebp-20h]
    float rada; // [esp+18h] [ebp-20h]
    float right[3]; // [esp+1Ch] [ebp-1Ch] BYREF
    float temp; // [esp+28h] [ebp-10h]
    float pitch; // [esp+2Ch] [ebp-Ch]
    float fCos; // [esp+30h] [ebp-8h]
    float fSin; // [esp+34h] [ebp-4h]

    vectosignedangles((const float *)axis, angles);
    right[0] = (*axis)[3];
    right[1] = (*axis)[4];
    right[2] = (*axis)[5];
    //rad = COERCE_FLOAT(*((unsigned int *)angles + 1) ^ _mask__NegFloat_) * 0.017453292;
    rad = -angles[1] * 0.017453292;
    fCos = cos(rad);
    fSin = sin(rad);
    temp = (float)(fCos * right[0]) - (float)(fSin * right[1]);
    right[1] = (float)(fSin * right[0]) + (float)(fCos * right[1]);
    //rada = COERCE_FLOAT(*(unsigned int *)angles ^ _mask__NegFloat_) * 0.017453292;
    rada = -angles[0] * 0.017453292;
    fCos = cos(rada);
    fSin = sin(rada);
    right[0] = (float)(fSin * right[2]) + (float)(fCos * temp);
    right[2] = (float)(fCos * right[2]) - (float)(fSin * temp);
    pitch = vectosignedpitch(right);
    if ( right[1] >= 0.0 )
    {
        //*((unsigned int *)angles + 2) = LODWORD(pitch) ^ _mask__NegFloat_;
        angles[2] = -pitch;
    }
    else
    {
        if ( pitch >= 0.0 )
            v2 = -180.0f;
        else
            v2 = 180.0f;
        angles[2] = pitch + v2;
    }
}

void __cdecl vectosignedangles(const float *vec, float *angles)
{
#if 0
    double v2; // xmm0_8
    double v3; // xmm0_8
    long double v4; // [esp-10h] [ebp-2Ch]
    long double v5; // [esp-10h] [ebp-2Ch]
    long double v6; // [esp-8h] [ebp-24h]
    long double v7; // [esp-8h] [ebp-24h]
    float v8; // [esp+4h] [ebp-18h]
    float v9; // [esp+8h] [ebp-14h]
    float v10; // [esp+Ch] [ebp-10h]

    if ( vec[1] == 0.0 && *vec == 0.0 )
    {
        v10 = 0.0f;
        //if ( COERCE_FLOAT(*((unsigned int *)vec + 2) ^ _mask__NegFloat_) < 0.0 )
        if ( -vec[2] < 0.0f)
            v9 = -90.0f;
        else
            v9 = 90.0f;
        v8 = v9;
    }
    else
    {
        *((float *)&v6 + 1) = *vec;
        v2 = vec[1];
        __libm_sse2_atan2(v4, v6);
        *(float *)&v2 = v2;
        v10 = (float)(*(float *)&v2 * 180.0) / 3.1415927;
        *(float *)&v7 = (float)(*vec * *vec) + (float)(vec[1] * vec[1]);
        HIDWORD(v5) = sqrtf(*(float *)&v7);
        *(float *)&v5 = vec[2];
        v3 = *(float *)&v5;
        __libm_sse2_atan2(v5, v7);
        *(float *)&v3 = v3;
        v8 = (float)(*(float *)&v3 * -180.0) / 3.1415927;
    }
    *angles = v8;
    angles[1] = v10;
    angles[2] = 0.0f;
#else // aislop
    const float x = vec[0];
    const float y = vec[1];
    const float z = vec[2];

    float pitch, yaw;

    // If pointing straight up/down (no horizontal component)
    if (x == 0.0f && y == 0.0f)
    {
        yaw = 0.0f;

        // Note: engine uses inverted pitch convention
        if (z > 0.0f)
            pitch = -90.0f;
        else
            pitch = 90.0f;
    }
    else
    {
        // Yaw = atan2(y, x)
        yaw = std::atan2(y, x) * (180.0f / 3.1415927f);

        // Forward length (horizontal magnitude)
        float forward = std::sqrt(x * x + y * y);

        // Pitch = -atan2(z, forward)
        pitch = -std::atan2(z, forward) * (180.0f / 3.1415927f);
    }

    angles[0] = pitch;
    angles[1] = yaw;
    angles[2] = 0.0f;
#endif
}

double __cdecl vectosignedpitch(const float *vec)
{
#ifdef KISAK_SP
    if (vec[1] == 0.0f && vec[0] == 0.0f)
        return -vec[2] >= 0.0f ? 90.0f : -90.0f;
    const float forward = std::sqrt(vec[1] * vec[1] + vec[0] * vec[0]);
    const float radians = (float)atan2((double)vec[2], (double)forward);
    return radians * -57.2957763671875f;
#else
    const float x = vec[0];
    const float y = vec[1];
    const float z = vec[2];
    if (x == 0.0f && y == 0.0f)
        return z > 0.0f ? -90.0 : 90.0;
    const float forward = std::sqrt(x * x + y * y);
    return -atan2(z, forward) * (180.0 / 3.14159265358979323846);
#endif
}

