#include "ui_credits_sp.h"

#ifdef KISAK_SP
#include "ui_main.h"
#include <database/database.h>
#include <gfx_d3d/r_rendercmds.h>
#include <qcommon/cmd.h>
#include <stringed/stringed_hooks.h>
#include <universal/com_stringtable.h>
#include <win32/win_shared.h>
#include <xmmintrin.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

const dvar_t *creditsScrollScale;

namespace
{
    struct CreditsRow_SP
    {
        const char *text;
        float color[4];
        int axis;
        int velocity;
        int x;
        int y;
        int width;
        int height;
        int movementMultiplier;
        int alignment;
        float scale;
    };
    static_assert(sizeof(CreditsRow_SP) == (sizeof(void *) == 4 ? 56 : 64), "typed credits row");

    XAssetHeader creditsTable;
    int creditsTableRow;
    int creditsActiveRows;
    bool creditsInputExhausted;
    CreditsRow_SP creditsRows[75];
    int creditsLastUpdateTime;

    int CreditsSignedWord_SP(std::uint32_t value)
    {
        int result;
        static_assert(sizeof(result) == sizeof(value), "retail DWORD width");
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    int CreditsAdd_SP(int a, int b)
    {
        return CreditsSignedWord_SP(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
    }

    int CreditsSubtract_SP(int a, int b)
    {
        return CreditsSignedWord_SP(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
    }

    int CreditsMultiply_SP(int a, int b)
    {
        return CreditsSignedWord_SP(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
    }

    int CreditsArithmeticHalf_SP(int value)
    {
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        return CreditsSignedWord_SP((bits >> 1) | (bits & 0x80000000u));
    }

    float CreditsFloat_SP(int value)
    {
        return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
    }

    float CreditsFloatAdd_SP(float a, float b)
    {
        return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }

    float CreditsFloatSubtract_SP(float a, float b)
    {
        return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }

    float CreditsFloatMultiply_SP(float a, float b)
    {
        return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }

    int CreditsTruncate_SP(float value)
    {
        return _mm_cvttss_si32(_mm_set_ss(value));
    }

    int CreditsColumnInt_SP(int column)
    {
        const int tableRow = creditsTableRow;
        const StringTable *table = creditsTable.stringTable;
        const char *value = StringTable_GetColumnValueForRow(table, tableRow, column);
        return std::atoi(value);
    }

    void CreditsNormalizeText_SP(char *destination, const char *text)
    {
        const int length = CreditsSignedWord_SP(static_cast<std::uint32_t>(std::strlen(text)));
        destination[1023] = 0;
        std::strncpy(destination, text, 1023);
        static const unsigned char selector[44] =
        {
            0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1, 2,
            2, 5, 2, 5, 3, 5, 5, 5, 5, 5, 5, 4
        };
        for (int i = 0; i < length; ++i)
        {
            const int index = static_cast<int>(static_cast<signed char>(destination[i])) + 55;
            if (static_cast<std::uint32_t>(index) <= 43u)
            {
                switch (selector[index])
                {
                case 0: destination[i] = 'E'; break;
                case 1: destination[i] = 'c'; break;
                case 2: destination[i] = 'e'; break;
                case 3: destination[i] = 'i'; break;
                case 4: destination[i] = 'o'; break;
                default: break;
                }
            }
        }
    }
}

void __cdecl UI_CreditsInit_SP()
{
    creditsInputExhausted = false;
    creditsActiveRows = 0;
    creditsTableRow = 0;
    StringTable_GetAsset("credits_pc.csv", &creditsTable);
    std::memset(creditsRows, 0, sizeof(creditsRows));
}

void __cdecl UI_DrawCredits_SP(
    int localClientNum,
    const rectDef_s *rect,
    Font_s *font,
    float scale,
    const float *color,
    int textStyle)
{
    (void)color;
    const int now = CreditsSignedWord_SP(static_cast<std::uint32_t>(Sys_Milliseconds()));
    if (CreditsSubtract_SP(now, creditsLastUpdateTime) >= 8)
    {
        creditsLastUpdateTime = now;
        int activeRows = creditsActiveRows;
        if (activeRows == 0)
        {
            if (creditsInputExhausted && ui_menuLvlNotify->current.enabled)
                Cbuf_AddText(localClientNum, "cmd mlvl credits done\n");
            UI_CreditsInit_SP();
            activeRows = creditsActiveRows;
        }

        int minY = 480;
        int minX = 640;
        int maxBottom = 0;
        int maxRight = 0;
        for (int i = 0; i < activeRows; ++i)
        {
            CreditsRow_SP &row = creditsRows[i];
            const int axis = row.axis;
            if (axis == 0 || axis == 1)
            {
                const int multiplier = row.movementMultiplier;
                const int velocity = row.velocity;
                const int movement = CreditsTruncate_SP(CreditsFloatMultiply_SP(
                    CreditsFloat_SP(CreditsMultiply_SP(multiplier, velocity)),
                    creditsScrollScale->current.value));
                if (axis == 0)
                    row.y = CreditsAdd_SP(row.y, movement);
                else
                    row.x = CreditsAdd_SP(row.x, movement);
            }

            const int bottom = CreditsAdd_SP(row.y, row.height);
            bool retire;
            int right = 0;
            if (row.velocity < 0)
            {
                retire = rect->y > CreditsFloat_SP(bottom);
                if (!retire)
                {
                    right = CreditsAdd_SP(row.width, row.x);
                    retire = rect->x > CreditsFloat_SP(right);
                }
            }
            else
            {
                retire = CreditsFloat_SP(bottom) >= CreditsFloatAdd_SP(rect->h, rect->y);
                if (!retire)
                {
                    right = CreditsAdd_SP(row.width, row.x);
                    retire = CreditsFloat_SP(right) >= CreditsFloatAdd_SP(rect->w, rect->x);
                }
            }
            if (retire)
            {
                std::memmove(&creditsRows[i], &creditsRows[i + 1],
                    static_cast<std::size_t>(activeRows - i - 1) * sizeof(CreditsRow_SP));
                activeRows = CreditsSubtract_SP(creditsActiveRows, 1);
                creditsActiveRows = activeRows;
                --i;
                continue;
            }
            if (row.y <= minY) minY = row.y;
            if (row.x <= minX) minX = row.x;
            if (bottom >= maxBottom) maxBottom = bottom;
            if (right >= maxRight) maxRight = right;
        }

        for (int slot = activeRows; slot < 75; ++slot)
        {
            const int tableRows = StringTable_RowCount(creditsTable.stringTable);
            if (tableRows <= creditsTableRow)
            {
                creditsInputExhausted = true;
                continue;
            }
            const int flags = CreditsColumnInt_SP(2);
            if (flags & 16)
            {
                for (; slot < 75; ++slot)
                {
                    CreditsRow_SP &row = creditsRows[slot];
                    row.text = "";
                    row.color[0] = row.color[1] = row.color[2] = row.color[3] = 1.0f;
                    row.axis = flags & 3;
                    row.velocity = CreditsColumnInt_SP(3);
                    row.width = CreditsColumnInt_SP(8);
                    row.height = CreditsColumnInt_SP(9);
                    row.x = CreditsColumnInt_SP(6);
                    row.y = CreditsAdd_SP(CreditsColumnInt_SP(7), CreditsMultiply_SP(row.height, slot));
                    row.movementMultiplier = CreditsColumnInt_SP(10);
                    const int alignment = CreditsColumnInt_SP(5);
                    creditsActiveRows = CreditsAdd_SP(creditsActiveRows, 1);
                    row.alignment = alignment;
                }
                creditsTableRow = CreditsAdd_SP(creditsTableRow, 1);
            }
            else
            {
                CreditsRow_SP &row = creditsRows[slot];
                const int tableRow = creditsTableRow;
                const StringTable *table = creditsTable.stringTable;
                row.text = StringTable_GetColumnValueForRow(table, tableRow, 1);
                if (*row.text == '@')
                    row.text = SEH_LocalizeTextMessage(row.text + 1, "credits", LOCMSG_SAFE);
                row.scale = CreditsFloatMultiply_SP(CreditsFloat_SP(CreditsColumnInt_SP(4)), 0.01f);
                row.color[0] = row.color[1] = row.color[2] = row.color[3] = 1.0f;
                row.axis = flags & 3;
                row.velocity = CreditsColumnInt_SP(3);
                row.width = CreditsColumnInt_SP(8);
                row.height = CreditsColumnInt_SP(9);
                if (row.height < 0)
                    row.height = UI_TextHeight(font, CreditsFloatMultiply_SP(row.scale, scale));
                row.y = (flags & 4) ? maxBottom : CreditsColumnInt_SP(7);
                row.x = (flags & 8) ? maxRight : CreditsColumnInt_SP(6);
                const int bottom = CreditsAdd_SP(row.height, row.y);
                if (bottom >= maxBottom) maxBottom = bottom;
                const int right = CreditsAdd_SP(row.width, row.x);
                if (right >= maxRight) maxRight = right;
                row.movementMultiplier = CreditsColumnInt_SP(10);
                const int alignment = CreditsColumnInt_SP(5);
                creditsTableRow = CreditsAdd_SP(creditsTableRow, 1);
                creditsActiveRows = CreditsAdd_SP(creditsActiveRows, 1);
                row.alignment = alignment;
            }
        }
    }

    rectDef_s viewport = *rect;
    const ScreenPlacement *placement = &scrPlaceView[localClientNum];
    ScrPlace_ApplyRect(placement, &viewport.x, &viewport.y, &viewport.w, &viewport.h,
        viewport.horzAlign, viewport.vertAlign);
    R_AddCmdSetScissorValues(true, CreditsTruncate_SP(viewport.x), CreditsTruncate_SP(viewport.y),
        CreditsTruncate_SP(viewport.w), CreditsTruncate_SP(viewport.h));
    for (int i = 0; i < creditsActiveRows; ++i)
    {
        CreditsRow_SP &row = creditsRows[i];
        float x;
        if (row.alignment == 1)
        {
            const int textWidth = UI_TextWidth(row.text, 0, font, CreditsFloatMultiply_SP(row.scale, scale));
            x = CreditsFloatSubtract_SP(CreditsFloatMultiply_SP(CreditsFloat_SP(row.width), 0.5f),
                CreditsFloat_SP(CreditsArithmeticHalf_SP(textWidth)));
            x = CreditsFloatAdd_SP(x, CreditsFloat_SP(row.x));
        }
        else if (row.alignment == 2)
        {
            const int textWidth = UI_TextWidth(row.text, 0, font, CreditsFloatMultiply_SP(row.scale, scale));
            x = CreditsFloat_SP(CreditsSubtract_SP(row.width, textWidth));
            x = CreditsFloatAdd_SP(x, CreditsFloat_SP(row.x));
        }
        else
        {
            x = CreditsFloat_SP(row.x);
        }
        x = CreditsFloatAdd_SP(x, rect->x);
        if (SEH_GetCurrentLanguage() == 11 || SEH_GetCurrentLanguage() == 13)
        {
            char text[1024];
            CreditsNormalizeText_SP(text, row.text);
            UI_DrawText(placement, text, 0x7FFFFFFF, font, x,
                CreditsFloatAdd_SP(CreditsFloat_SP(row.y), rect->y),
                rect->horzAlign, rect->vertAlign, CreditsFloatMultiply_SP(row.scale, scale), row.color, textStyle);
        }
        else
        {
            UI_DrawText(placement, const_cast<char *>(row.text), 0x7FFFFFFF, font, x,
                CreditsFloatAdd_SP(CreditsFloat_SP(row.y), rect->y),
                rect->horzAlign, rect->vertAlign, CreditsFloatMultiply_SP(row.scale, scale), row.color, textStyle);
        }
    }
    R_AddCmdSetScissorValues(false, 0, 0, 0, 0);
}
#endif
