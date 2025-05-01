/* -*- C++ -*-
 *
 *  PonscripterLabel_effect.cpp - Effect executer of Ponscripter
 *
 *  Copyright (c) 2001-2009 Ogapee (original ONScripter, of which this
 *  is a fork).
 *
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 */

#include "PonscripterLabel.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <omp.h>
AcceleratedGraphicsFunctions gfx2;

#define EFFECT_STRIPE_WIDTH (16 * screen_ratio1 / screen_ratio2)
#define EFFECT_STRIPE_CURTAIN_WIDTH (24 * screen_ratio1 / screen_ratio2)
#define EFFECT_QUAKE_AMP (12 * screen_ratio1 / screen_ratio2)

static char *dll=NULL, *params=NULL; //for dll-based effects

int PonscripterLabel::setEffect(Effect& effect, bool generate_effect_dst, bool update_backup_surface)
{
    doing_effect = true;
    setEffect_flag = true;
    if (effect.effect == 0) return RET_CONTINUE;

    if (update_backup_surface)
        refreshSurface(backup_surface, &dirty_rect.bounding_box,
                       REFRESH_NORMAL_MODE);
    
    int effect_no = effect.effect;
    if (effect_cut_flag && skip_flag) effect_no = 1;

    //if(effect_flag && !(skip_flag || ctrl_pressed_status || skip_to_wait)) SDL_BlitSurface(simul_src_surface, NULL, effect_src_surface, NULL);
    if(effect_flag) SDL_BlitSurface(simul_src_surface, NULL, effect_src_surface, NULL);
    else SDL_BlitSurface(accumulation_surface, NULL, effect_src_surface, NULL);

    if (generate_effect_dst){
        int refresh_mode = refreshMode();
        if (update_backup_surface && refresh_mode == REFRESH_NORMAL_MODE){
            SDL_BlitSurface(backup_surface, &dirty_rect.bounding_box,
                            effect_dst_surface, &dirty_rect.bounding_box);
        }
        else {
            if (effect_no == 1)
                refreshSurface(effect_dst_surface, &dirty_rect.bounding_box,
                               refresh_mode);
            else
                refreshSurface(effect_dst_surface, NULL, refresh_mode);
        }
    }
    setEffect_flag = false;

    /* Load mask image */
    if (effect_no == 15 || effect_no == 18){
        if (!effect.anim.image_surface){
            parseTaggedString(&effect.anim, true);
            setupAnimationInfo(&effect.anim);
        }
    }
    if (effect_no == 11 || effect_no == 12 || effect_no == 13 ||
        effect_no == 14 || effect_no == 16 || effect_no == 17)
        dirty_rect.fill( screen_width, screen_height );

    dll = params = NULL;
    if (effect_no == 99) { // dll-based
        dll = bstr2cstr(&effect.anim.image_name, '0'); // TODO: Make dll a bstring natively
        if (dll != NULL) { //just in case no dll is given
            if (debug_level > 0)
                printf("dll effect: Got dll/params '%s'\n", dll);

            params = dll;
            while (*params != 0 && *params != '/') params++;
            if (*params == '/') params++;

            if (!strncmp(dll, "whirl.dll", 9)) {
                buildSinTable();
                buildCosTable();
                buildWhirlTable();
                dirty_rect.fill( screen_width, screen_height );
            }
            else if (!strncmp(dll, "trvswave.dll", 12)) {
                buildSinTable();
                dirty_rect.fill( screen_width, screen_height );
            }
            else if (!strncmp(dll, "breakup.dll", 11)) {
                initBreakup(params);
                dirty_rect.fill( screen_width, screen_height );
            }
            else if (!strncmp(dll, "glass.dll", 9)) {
                initGlass();
                dirty_rect.fill( screen_width, screen_height );
            }
            else {
                dirty_rect.fill( screen_width, screen_height );
            }
        }
    }

    effect_counter = 0;
    effect_start_time_old = SDL_GetTicks();
    event_mode = EFFECT_EVENT_MODE;
    advancePhase();

    return RET_WAIT | RET_REREAD;
}


int PonscripterLabel::doEffect(Effect& effect, bool clear_dirty_region)
{
    if (lastRenderEvent < RENDER_EVENT_EFFECT) { lastRenderEvent = RENDER_EVENT_EFFECT; }

    bool first_time = (effect_counter == 0);

    int prevduration = effect.duration;
    if (ctrl_pressed_status || skip_to_wait) {
        effect.duration = effect_counter = 1;
    }

    effect_start_time = SDL_GetTicks();

    effect_timer_resolution = effect_start_time - effect_start_time_old;
    effect_start_time_old = effect_start_time;

    int effect_no = effect.effect;
    if (effect_cut_flag && skip_flag) effect_no = 1;

    int i;
    int width, width2;
    int height, height2;
    SDL_Rect src_rect = { 0, 0, screen_width, screen_height };
    SDL_Rect dst_rect = { 0, 0, screen_width, screen_height };

    /* ---------------------------------------- */
    /* Execute effect */
    //printf("Effect number %d %d\n", effect_no, effect->duration );

    doing_effect = true;

    switch (effect_no) {
    case 0: // Instant display
    case 1: // Instant display
        //drawEffect( &src_rect, &src_rect, effect_dst_surface );
        break;

    case 2: // Left shutter
        width = EFFECT_STRIPE_WIDTH * effect_counter / effect.duration;
        for (i = 0; i < screen_width / EFFECT_STRIPE_WIDTH; i++) {
            src_rect.x = i * EFFECT_STRIPE_WIDTH;
            src_rect.y = 0;
            src_rect.w = width;
            src_rect.h = screen_height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }

        break;

    case 3: // Right shutter
        width = EFFECT_STRIPE_WIDTH * effect_counter / effect.duration;
        for (i = 1; i <= screen_width / EFFECT_STRIPE_WIDTH; i++) {
            src_rect.x = i * EFFECT_STRIPE_WIDTH - width - 1;
            src_rect.y = 0;
            src_rect.w = width;
            src_rect.h = screen_height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }

        break;

    case 4: // Top shutter
        height = EFFECT_STRIPE_WIDTH * effect_counter / effect.duration;
        for (i = 0; i < screen_height / EFFECT_STRIPE_WIDTH; i++) {
            src_rect.x = 0;
            src_rect.y = i * EFFECT_STRIPE_WIDTH;
            src_rect.w = screen_width;
            src_rect.h = height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }

        break;

    case 5: // Bottom shutter
        height = EFFECT_STRIPE_WIDTH * effect_counter / effect.duration;
        for (i = 1; i <= screen_height / EFFECT_STRIPE_WIDTH; i++) {
            src_rect.x = 0;
            src_rect.y = i * EFFECT_STRIPE_WIDTH - height - 1;
            src_rect.w = screen_width;
            src_rect.h = height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }

        break;

    case 6: // Left curtain
        width = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect.duration;
        for (i = 0; i <= screen_width / EFFECT_STRIPE_CURTAIN_WIDTH; i++) {
            width2 = width - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_width;
            if (width2 >= 0) {
                src_rect.x = i * EFFECT_STRIPE_CURTAIN_WIDTH;
                src_rect.y = 0;
                src_rect.w = width2;
                src_rect.h = screen_height;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }

        break;

    case 7: // Right curtain
        width = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect.duration;
        for (i = 0; i <= screen_width / EFFECT_STRIPE_CURTAIN_WIDTH; i++) {
            width2 = width - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_width;
            if (width2 >= 0) {
                if (width2 > EFFECT_STRIPE_CURTAIN_WIDTH) width2 = EFFECT_STRIPE_CURTAIN_WIDTH;

                src_rect.x = screen_width - i * EFFECT_STRIPE_CURTAIN_WIDTH - width2;
                src_rect.y = 0;
                src_rect.w = width2;
                src_rect.h = screen_height;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }

        break;

    case 8: // Top curtain
        height = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect.duration;
        for (i = 0; i <= screen_height / EFFECT_STRIPE_CURTAIN_WIDTH; i++) {
            height2 = height - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_height;
            if (height2 >= 0) {
                src_rect.x = 0;
                src_rect.y = i * EFFECT_STRIPE_CURTAIN_WIDTH;
                src_rect.w = screen_width;
                src_rect.h = height2;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }

        break;

    case 9: // Bottom curtain
        height = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect.duration;
        for (i = 0; i <= screen_height / EFFECT_STRIPE_CURTAIN_WIDTH; i++) {
            height2 = height - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_height;
            if (height2 >= 0) {
                src_rect.x = 0;
                src_rect.y = screen_height - i * EFFECT_STRIPE_CURTAIN_WIDTH - height2;
                src_rect.w = screen_width;
                src_rect.h = height2;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }

        break;

    case 99: // dll-based
        if (dll != NULL) {
            if (!strncmp(dll, "cascade.dll", 11)) {
                effectCascade(params, effect.duration);
                break;
            } else if (!strncmp(dll, "whirl.dll", 9)) {
                effectWhirl(params, effect.duration);
                break;
            } else if (!strncmp(dll, "trvswave.dll", 12)) {
                effectTrvswave(params, effect.duration);
                break;
            } else if (!strncmp(dll, "breakup.dll", 11)) {
                effectBreakup(params, effect.duration);
                break;
            } else if (!strncmp(dll, "glass.dll", 9)) {
                effectGlass(params, effect.duration);
                break;
            } else {
                if (first_time) {
                    printf("Effect %d, DLL emulation not found: %s\n", effect_no, dll);
                }
            }
        } else { //just in case no dll is given
            if (first_time) {
                printf("effect No. %d, but no DLL name supplied.\n", effect_no);
            }
        }

        // fall through to default case

    default:
        if (first_time) {
            printf("effect No. %d is not implemented. Crossfade is substituted for that.\n", effect_no);
        }

    case 10: // Cross fade
        height = 256 * effect_counter / effect.duration;
        alphaMaskBlend(NULL, ALPHA_BLEND_CONST, height, &dirty_rect.bounding_box);
        break;

    case 11: // Left scroll
        width = screen_width * effect_counter / effect.duration;
        src_rect.x = 0;
        dst_rect.x = width;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width - width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = screen_width - width - 1;
        dst_rect.x = 0;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

    case 12: // Right scroll
        width = screen_width * effect_counter / effect.duration;
        src_rect.x = width;
        dst_rect.x = 0;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width - width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = 0;
        dst_rect.x = screen_width - width - 1;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

    case 13: // Top scroll
        width = screen_height * effect_counter / effect.duration;
        src_rect.x = dst_rect.x = 0;
        src_rect.y = 0;
        dst_rect.y = width;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = screen_height - width;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = dst_rect.x = 0;
        src_rect.y = screen_height - width - 1;
        dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = width;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

    case 14: // Bottom scroll
        width = screen_height * effect_counter / effect.duration;
        src_rect.x = dst_rect.x = 0;
        src_rect.y = width;
        dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = screen_height - width;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = dst_rect.x = 0;
        src_rect.y = 0;
        dst_rect.y = screen_height - width - 1;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = width;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

    case 15: // Fade with mask
        alphaMaskBlend(effect.anim.image_surface, ALPHA_BLEND_FADE_MASK, 256 * effect_counter / effect.duration, &dirty_rect.bounding_box);
        break;

    case 16: // Mosaic out
        generateMosaic(effect_src_surface, 5 - 6 * effect_counter / effect.duration);
        break;

    case 17: // Mosaic in
        generateMosaic(effect_dst_surface, 6 * effect_counter / effect.duration);
        break;

    case 18: // Cross fade with mask
        alphaMaskBlend(effect.anim.image_surface, ALPHA_BLEND_CROSSFADE_MASK, 256 * effect_counter * 2 / effect.duration, &dirty_rect.bounding_box);
        break;

    case (CUSTOM_EFFECT_NO + 0): // quakey
        if (effect_timer_resolution > effect.duration / 4 / effect.no)
            effect_timer_resolution = effect.duration / 4 / effect.no;

        dst_rect.x = 0;
        dst_rect.y = (Sint16) (sin(M_PI * 2.0 * effect.no * effect_counter / effect.duration) *
                               EFFECT_QUAKE_AMP * effect.no * (effect.duration - effect_counter) / effect.duration);
        SDL_FillRect(accumulation_surface, NULL, SDL_MapRGBA(accumulation_surface->format, 0, 0, 0, 0xff));
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

    case (CUSTOM_EFFECT_NO + 1): // quakex
        if (effect_timer_resolution > effect.duration / 4 / effect.no)
            effect_timer_resolution = effect.duration / 4 / effect.no;

        dst_rect.x = (Sint16) (sin(M_PI * 2.0 * effect.no * effect_counter / effect.duration) *
                               EFFECT_QUAKE_AMP * effect.no * (effect.duration - effect_counter) / effect.duration);
        dst_rect.y = 0;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

    case (CUSTOM_EFFECT_NO + 2): // quake
        dst_rect.x = effect.no * get_rnd(-1, 1) * 2;
        dst_rect.y = effect.no * get_rnd(-1, 1) * 2;
        SDL_FillRect(accumulation_surface, NULL, SDL_MapRGBA(accumulation_surface->format, 0, 0, 0, 0xff));
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;
    }

    //printf("effect conut %d / dur %d\n", effect_counter, effect.duration);
    //SDL_BlitSurface(accumulation_surface, NULL, simul_src_surface, NULL);
    SDL_BlitSurface(effect_dst_surface, &dirty_rect.bounding_box, simul_src_surface, &dirty_rect.bounding_box);
    SDL_Rect temp_rect={0, 0, screen_width, screen_height};
    simul_checkFlushSub();
    //simul_refreshSub(accumulation_surface, temp_rect, -1);

    effect_counter += effect_timer_resolution;
    if (effect_counter < effect.duration && effect_no != 1) {
        simul_refreshSub(accumulation_surface, temp_rect, -1);
        if (effect_no)
            flush(REFRESH_NONE_MODE, NULL, false);
        effect.duration = prevduration;
        return RET_WAIT | RET_REREAD;
    }
    else {
        SDL_BlitSurface(effect_dst_surface, &dirty_rect.bounding_box,
			accumulation_surface, &dirty_rect.bounding_box);
        simul_refreshSub(accumulation_surface, temp_rect, -1);
        doing_effect = false;
        simul_checkFlush();
        if (effect_no)
	    flush(REFRESH_NONE_MODE, NULL, clear_dirty_region);
        if (effect_no == 1)
	    effect_counter = 0;
    
        if(lip_effpause_flag){
            //simul_Channel[0].mode = 1;
            //lip_effpause_flag = 0;
        }

        effect.duration = prevduration;
        event_mode = IDLE_EVENT_MODE;

        return RET_CONTINUE;
    }
}


void PonscripterLabel::drawEffect(SDL_Rect* dst_rect, SDL_Rect* src_rect, SDL_Surface* surface)
{
    SDL_Rect clipped_rect;
    if (AnimationInfo::doClipping(dst_rect, &dirty_rect.bounding_box, &clipped_rect)) return;

    if (src_rect != dst_rect) {
        src_rect->x += clipped_rect.x;
        src_rect->y += clipped_rect.y;
        src_rect->w  = clipped_rect.w;
        src_rect->h  = clipped_rect.h;
    }

    SDL_BlitSurface(surface, src_rect, accumulation_surface, dst_rect);
}


void PonscripterLabel::generateMosaic(SDL_Surface* src_surface, int level)
{
    int i, j, ii, jj;
    int width = 160;
    for (i = 0; i < level; i++) width >>= 1;

#ifdef BPP16
    int total_width = accumulation_surface->pitch / 2;
#else
    int total_width = accumulation_surface->pitch / 4;
#endif
    SDL_LockSurface(src_surface);
    SDL_LockSurface(accumulation_surface);
    ONSBuf* src_buffer = (ONSBuf*) src_surface->pixels;

    for (i = screen_height - 1; i >= 0; i -= width) {
        for (j = 0; j < screen_width; j += width) {
            ONSBuf  p = src_buffer[i * total_width + j];
            ONSBuf* dst_buffer = (ONSBuf*) accumulation_surface->pixels + i * total_width + j;

            int height2 = width;
            if (i + 1 - width < 0) height2 = i + 1;

            int width2 = width;
            if (j + width > screen_width) width2 = screen_width - j;

            for (ii = 0; ii < height2; ii++) {
                for (jj = 0; jj < width2; jj++) {
                    *dst_buffer++ = p;
                }

                dst_buffer -= total_width + width2;
            }
        }
    }

    SDL_UnlockSurface(accumulation_surface);
    SDL_UnlockSurface(src_surface);
}

struct Point2D {
    float x, y;
    Point2D(float x = 0.0, float y = 0.0) : x(x), y(y) {}
};

struct Point3D {
    float x, y, z;
    Point3D(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
};

struct Triangle {
    Point2D p1, p2, p3;
};

/*
struct Triangle2 {
    Point2D p1, p2, p3;
    // 사전 계산된 값 (선택적)
    double xmin, xmax, ymin, ymax; // 바운딩 박스
    double dx12, dy12, dx23, dy23, dx31, dy31; // 변 벡터

    // 생성자에서 사전 계산
    Triangle2(Point2D p1, Point2D p2, Point2D p3) : p1(p1), p2(p2), p3(p3) {
        // 바운딩 박스
        xmin = std::min({p1.x, p2.x, p3.x});
        xmax = std::max({p1.x, p2.x, p3.x});
        ymin = std::min({p1.y, p2.y, p3.y});
        ymax = std::max({p1.y, p2.y, p3.y});
        // 변 벡터
        dx12 = p2.x - p1.x; dy12 = p2.y - p1.y;
        dx23 = p3.x - p2.x; dy23 = p3.y - p2.y;
        dx31 = p1.x - p3.x; dy31 = p1.y - p3.y;
    }
};
*/

// 중심좌표 구조체
struct Barycentric {
    float u, v, w;
};

// === 2. 사전 계산된 삼각형 정보 구조체 ===
struct PrecomputedTriangle {
    Point2D p1, p2, p3; // 변환된 꼭짓점 좌표 (float)
    float xmin, xmax, ymin, ymax; // 바운딩 박스 (float)
    float invDenom;       // 무게중심 계산 분모의 역수 (float)
    // 무게중심 계산 위한 사전 계산 계수들 (float)
    float C1x, C1y; // u 계산용 (y2-y3), (x3-x2)
    float C2x, C2y; // v 계산용 (y3-y1), (x1-x3)
    float p3x, p3y; // 자주 사용될 p3 좌표 캐싱
    // 기본 생성자 추가
    PrecomputedTriangle() 
        : p1(0.0f, 0.0f), p2(0.0f, 0.0f), p3(0.0f, 0.0f),
        xmin(0.0f), xmax(0.0f), ymin(0.0f), ymax(0.0f),
        invDenom(0.0f), C1x(0.0f), C1y(0.0f), C2x(0.0f), C2y(0.0f),
        p3x(0.0f), p3y(0.0f) {}
    // 생성자에서 사전 계산 수행
    PrecomputedTriangle(const Point2D& pt1, const Point2D& pt2, const Point2D& pt3)
        : p1(pt1), p2(pt2), p3(pt3)
    {
        // 바운딩 박스 계산
        xmin = std::min({p1.x, p2.x, p3.x});
        xmax = std::max({p1.x, p2.x, p3.x});
        ymin = std::min({p1.y, p2.y, p3.y});
        ymax = std::max({p1.y, p2.y, p3.y});

        // 무게중심 계산용 값 사전 계산
        p3x = p3.x;
        p3y = p3.y;
        C1x = p2.y - p3.y; C1y = p3.x - p2.x;
        C2x = p3.y - p1.y; C2y = p1.x - p3.x;

        float denominator = C1x * (p1.x - p3.x) + C1y * (p1.y - p3.y);

        // 매우 작은 분모 처리 (퇴화 삼각형 등)
        invDenom = (std::abs(denominator) < 1e-8f) ? 0.0f : 1.0f / denominator;
    }

    // 점 p가 삼각형 내부에 있으면 true를 반환하고 out_bary에 좌표를 저장
    bool computeBarycentricIfInside(const Point2D& p, Barycentric& out_bary) const {
        if (invDenom == 0.0f) return false;

        float u = (C1x * (p.x - p3x) + C1y * (p.y - p3y)) * invDenom;
        float v = (C2x * (p.x - p3x) + C2y * (p.y - p3y)) * invDenom;
        float w = 1.0f - u - v;

        if (u >= 0.0f && v >= 0.0f && w >= 0.0f) {
            out_bary = {u, v, w}; // 출력 파라미터에 결과 저장
            return true;          // 성공 반환
        }

        return false; // 실패 반환
    }
};

// 3D 벡터 행렬 곱 (3x3 행렬)
Point3D matrixMultiply(const float R[3][3], const Point3D& p) {
    Point3D result;
    result.x = R[0][0] * p.x + R[0][1] * p.y + R[0][2] * p.z;
    result.y = R[1][0] * p.x + R[1][1] * p.y + R[1][2] * p.z;
    result.z = R[2][0] * p.x + R[2][1] * p.y + R[2][2] * p.z;
    return result;
}

// 랜덤 단위 벡터 생성
Point3D randomUnitVector() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0f, 1.0f);

    Point3D v;
    float lengthSq;
    do {
        v.x = dis(gen);
        v.y = dis(gen);
        v.z = dis(gen);
        lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    } while (lengthSq > 1.0f || lengthSq == 0.0f); // Avoid zero vector

    float length = std::sqrt(lengthSq);
    v.x /= length;
    v.y /= length;
    v.z /= length;
    return v;
}

// 회전 행렬 생성
void createRotationMatrix(float R[3][3], const Point3D& axis, float theta) {
    float c = std::cos(theta);
    float s = std::sin(theta);
    float t = 1.0f - c;
    float x = axis.x, y = axis.y, z = axis.z;

    R[0][0] = t * x * x + c;
    R[0][1] = t * x * y - s * z;
    R[0][2] = t * x * z + s * y;
    R[1][0] = t * y * x + s * z;
    R[1][1] = t * y * y + c;
    R[1][2] = t * y * z - s * x;
    R[2][0] = t * z * x - s * y;
    R[2][1] = t * z * y + s * x;
    R[2][2] = t * z * z + c;
}

// 중심좌표 계산
/*
Barycentric computeBarycentric(const Triangle& tri, const Point2D& p) {
    float x1 = tri.p1.x, y1 = tri.p1.y;
    float x2 = tri.p2.x, y2 = tri.p2.y;
    float x3 = tri.p3.x, y3 = tri.p3.y;
    float x = p.x, y = p.y;

    float denominator = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);
    if (std::abs(denominator) < 1e-10) {
        return {0, 0, 0}; // 삼각형이 유효하지 않음
    }

    float u = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / denominator;
    float v = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / denominator;
    float w = 1.0 - u - v;

    return {u, v, w};
}
*/

// 삼각형 회전 (변환된 삼각형 생성)
Triangle rotateTriangle(const Triangle& tri, const Point3D& axis, float theta) {
    Point2D centroid = {
        (tri.p1.x + tri.p2.x + tri.p3.x) / 3.0f,
        (tri.p1.y + tri.p2.y + tri.p3.y) / 3.0f
    };

    Point3D p1_3d = {tri.p1.x - centroid.x, tri.p1.y - centroid.y, 0.0f};
    Point3D p2_3d = {tri.p2.x - centroid.x, tri.p2.y - centroid.y, 0.0f};
    Point3D p3_3d = {tri.p3.x - centroid.x, tri.p3.y - centroid.y, 0.0f};

    float R[3][3];
    createRotationMatrix(R, axis, theta);

    Point3D p1_rot = matrixMultiply(R, p1_3d);
    Point3D p2_rot = matrixMultiply(R, p2_3d);
    Point3D p3_rot = matrixMultiply(R, p3_3d);

    Triangle result;
    result.p1 = {p1_rot.x + centroid.x, p1_rot.y + centroid.y};
    result.p2 = {p2_rot.x + centroid.x, p2_rot.y + centroid.y};
    result.p3 = {p3_rot.x + centroid.x, p3_rot.y + centroid.y};

    return result;
}

// 변환된 점에서 원본 점 계산
/*
Point2D inverseTransformPoint(const Triangle& origTri, const Triangle& transformedTri, const Point2D& p_prime) {
    // 변환된 삼각형에서 중심좌표 계산
    Barycentric bary = computeBarycentric(transformedTri, p_prime);

    // 원본 삼각형에 중심좌표 적용
    Point2D result;
    result.x = bary.u * origTri.p1.x + bary.v * origTri.p2.x + bary.w * origTri.p3.x;
    result.y = bary.u * origTri.p1.y + bary.v * origTri.p2.y + bary.w * origTri.p3.y;

    return result;
}
*/
/*
bool isPointInsideTriangle(const Triangle2& tri, const Point2D& p) {
    // 바운딩 박스 테스트
    if (p.x < tri.xmin || p.x > tri.xmax || p.y < tri.ymin || p.y > tri.ymax) {
        return false;
    }

    // 동일 방향 테스트 (외적 부호 확인)
    double s1 = tri.dx12 * (p.y - tri.p1.y) - tri.dy12 * (p.x - tri.p1.x); // P1P2
    double s2 = tri.dx23 * (p.y - tri.p2.y) - tri.dy23 * (p.x - tri.p2.x); // P2P3
    double s3 = tri.dx31 * (p.y - tri.p3.y) - tri.dy31 * (p.x - tri.p3.x); // P3P1

    // 모두 양수 또는 0 (경계 포함)
    return s1 >= 0 && s2 >= 0 && s3 >= 0;
}
*/

/*
bool isPointInsideTriangleBarycentric(const Triangle& tri, const Point2D& p) {
    float x1 = tri.p1.x, y1 = tri.p1.y;
    float x2 = tri.p2.x, y2 = tri.p2.y;
    float x3 = tri.p3.x, y3 = tri.p3.y;
    float x = p.x, y = p.y;

    float D = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);
    if (std::abs(D) < 1e-10) return false;

    float u = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / D;
    float v = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / D;
    float w = 1.0 - u - v;

    return u >= 0 && v >= 0 && w >= 0;
}
*/

inline float multPoint(float x, float mlt){
    return 0.5 + (x - 0.5) * mlt;
}

inline float divPoint(float x, float mlt){
    return 0.5 + (x - 0.5) / mlt;
}

#define glassN 6
#define triN 72
#define multXY 0.7f
#define moveXY 0.4f
#define upXY 0.2f
#define downXY 1.2f
#define blurN 3
Triangle glass_tri[triN];
Point3D glass_axis[triN];
float glass_theta[triN];

void PonscripterLabel::initGlass(){
    //std::random_device rd;
    //std::mt19937 gen(rd());
    std::mt19937 gen((unsigned int)time(NULL));
    std::uniform_real_distribution<> dis(-0.5f, 0.5f);
    std::uniform_real_distribution<> dis2(0.5f*M_PI, 2.0f*M_PI);
    float dx, dy;
    int i, j, k;
    int trii=0;
    Point2D dp[2][glassN+1];
    for(i=0; i <= glassN; i++){
        for(j=0; j <= glassN; j++){
            dx = (float)i * 1.0f / glassN;
            dy = (float)j * 1.0f / glassN;
            if(i!=0 && i!=glassN){
                dx += dis(gen) / glassN;
            }
            if(j!=0 && j!=glassN){
                dy += dis(gen) / glassN;
            }
            dp[i%2][j] = {dx, dy};
            if(i && j){
                k=0;
                if(dis(gen)>0.0f) k=1;
                glass_tri[trii]={{dp[(i-1)%2][j-1].x, dp[(i-1)%2][j-1].y}, {dp[(i-1)%2][j].x, dp[(i-1)%2][j].y}, {dp[i%2][j-k].x, dp[i%2][j-k].y}};
                glass_axis[trii] = randomUnitVector();
                glass_theta[trii++] = dis2(gen);
                glass_tri[trii]={{dp[i%2][j-1].x, dp[i%2][j-1].y}, {dp[i%2][j].x, dp[i%2][j].y}, {dp[(i-1)%2][j-(1-k)].x, dp[(i-1)%2][j-(1-k)].y}};
                glass_axis[trii] = randomUnitVector();
                glass_theta[trii++] = dis2(gen);
            }
        }
    }
}

// bg Mlib_1br,99,1000,"glass.dll"
void PonscripterLabel::effectGlass( char *params, int duration )
{
    SDL_BlitSurface(effect_dst_surface, NULL, accumulation_surface, NULL);

    SDL_LockSurface( effect_src_surface );
    SDL_LockSurface( accumulation_surface );
    ONSBuf *src_buffer = (ONSBuf *)effect_src_surface->pixels;
    ONSBuf *dst_buffer_start = (ONSBuf *)accumulation_surface->pixels;
    //ONSBuf *dst_buffer = (ONSBuf *)accumulation_surface->pixels;
    ONSBuf *temp_buffer;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    unsigned char* alphap_base = (unsigned char*) src_buffer + 3;
#else
    unsigned char* alphap_base = (unsigned char*) src_buffer;
#endif
    int alpha = 256;

    // === 4. 삼각형 변환 및 사전 계산 단계 ===
    // 각 프레임마다 변환될 삼각형 정보를 저장할 벡터 (PrecomputedTriangle 사용)
    PrecomputedTriangle precomputedTris[triN];
    //precomputedTris.reserve(triN); // 미리 메모리 할당 (재할당 방지)

    // 현재 진행률 계산 (0.0 ~ 1.0)
    float current_progress = (duration > 0) ? (float)effect_counter / (float)duration : 0.0f;
    alpha = (int)(256.0f*std::sqrt(1.0f - current_progress));

    // 현재 진행률에 따른 회전 외 추가 변환 계수 계산
    float mlt_rot = 1.0f + moveXY * current_progress; // 확대/이동 관련
    // 아래로 떨어지는 포물선 이동량 계산 (upXY에서 시작)
    float down_shift = downXY * (std::pow(current_progress - upXY, 2.0f) - std::pow(upXY, 2.0f));

    // 모든 원본 삼각형에 대해 변환 수행
    for(int k=0; k<triN; k++){
        // 1. 원본 삼각형(glass_tri)을 현재 진행률에 맞게 회전
        Triangle rotatedTri = rotateTriangle(glass_tri[k], glass_axis[k], glass_theta[k] * current_progress);

        // 2. 추가 변환 (이동, 확대 등) 적용: 각 꼭지점에 적용
        // Y축으로 떨어지는 이동량 적용
            Point2D p_shift = {multPoint(rotatedTri.p3.x, mlt_rot) - rotatedTri.p3.x, multPoint(rotatedTri.p3.y, mlt_rot) - rotatedTri.p3.y + down_shift};
            
            // 각 꼭지점을 multPoint로 확대/이동하고, y축 이동량 추가
            Point2D p1_transformed = {rotatedTri.p1.x + p_shift.x, rotatedTri.p1.y + p_shift.y};
            Point2D p2_transformed = {rotatedTri.p2.x + p_shift.x, rotatedTri.p2.y + p_shift.y};
            Point2D p3_transformed = {rotatedTri.p3.x + p_shift.x, rotatedTri.p3.y + p_shift.y};

        // 3. 변환된 꼭지점들로 PrecomputedTriangle 객체 생성
        //    이 과정에서 바운딩 박스, invDenom 등 사전 계산이 자동으로 수행됨
        precomputedTris[k] = PrecomputedTriangle(p1_transformed, p2_transformed, p3_transformed);
    }

    float mlt_map = std::pow(1.0f + multXY * current_progress, 2);
    float inv_mlt_map = (std::abs(mlt_map) > 1e-8f) ? 1.0f / mlt_map : 1.0f;

    // 화면 크기 역수 미리 계산 (픽셀 좌표 정규화 시 곱셈 사용 위함)
    const float inv_screen_width = 1.0f / (float)screen_width;
    const float inv_screen_height = 1.0f / (float)screen_height;

    // === 5. 픽셀 루프 (OpenMP 병렬 처리 적용) ===
    // OpenMP 지시어: 병렬 루프 실행, 각 스레드는 지정된 변수들의 private 복사본 가짐
    //#pragma omp parallel for private(i, j, k, p, p2, bary, temp_buffer, alphap) schedule(static) // 스케줄링 방식 지정 가능 (예: static, dynamic)
    #pragma omp parallel for private(i, j, k, p, p2, bary, temp_buffer) schedule(static)
    for ( int i=0 ; i<screen_height ; i+=blurN ){
            // 현재 행의 대상 버퍼 시작 포인터 계산
            ONSBuf *dst_row_ptr = dst_buffer_start + i * screen_width;
            // 현재 행의 알파 포인터 시작 주소 계산 (픽셀당 4바이트 가정)
            //unsigned char* alphap_row = alphap_base + i * screen_width * 4;

        for ( int j=0 ; j<screen_width ; j+=blurN){
            ONSBuf *dst_buffer = dst_row_ptr + j;
            //unsigned char* alphap = alphap_row + j * 4;

            // 현재 픽셀 (블록의 좌상단)의 정규화된 좌표 (0.0 ~ 1.0) 계산
            //float px_norm = (float)j * inv_screen_width;
            //float py_norm = (float)i * inv_screen_height;
            // 정규화된 좌표를 divPoint 함수를 통해 변환 (원본 좌표계에서의 위치 추정)
            // Point2D p = {divPoint(px_norm, mlt_map), divPoint(py_norm, mlt_map)};
            // 나눗셈 대신 미리 계산한 역수 사용
            Point2D p = {0.5f + ((float)j * inv_screen_width - 0.5f) * inv_mlt_map, 0.5f + ((float)i * inv_screen_height - 0.5f) * inv_mlt_map};


            bool pixel_processed = false; // 현재 3x3 블록이 처리되었는지 여부 플래그
            Barycentric bary; // 무게중심 좌표를 저장할 변수 (루프 내에서 재사용)

            // 현재 픽셀 p에 대해 모든 변환된 삼각형 검사
            for(int k=0; k<triN; ++k){ // Use ++k for slightly better performance potential
                // 사전 계산된 삼각형 정보 참조
                const auto& pt = precomputedTris[k];

                // 1. 바운딩 박스 검사: 픽셀 p가 삼각형의 바운딩 박스 내에 있는지 빠르게 확인
                // 경계 포함 (<=)으로 일관성 유지
                if(p.x >= pt.xmin && p.x <= pt.xmax && p.y >= pt.ymin && p.y <= pt.ymax) {

                    // 2. 최적화된 무게중심 계산 및 내부 판별 함수 호출
                    //    성공 시 true 반환, bary 변수에 계산된 좌표 채워짐
                    if (pt.computeBarycentricIfInside(p, bary)) {

                        // --- 점이 삼각형 내부에 있는 경우 ---

                        // 3. 계산된 무게중심 좌표(bary)를 사용해 원본 삼각형(glass_tri)에서의 좌표(p2) 계산
                        Point2D p2; // 원본 텍스처 좌표
                        p2.x = bary.u * glass_tri[k].p1.x + bary.v * glass_tri[k].p2.x + bary.w * glass_tri[k].p3.x;
                        p2.y = bary.u * glass_tri[k].p1.y + bary.v * glass_tri[k].p2.y + bary.w * glass_tri[k].p3.y;

                        // 4. 원본 이미지 버퍼(src_buffer)에서 가져올 픽셀 좌표 계산
                        //    정규화된 좌표 p2를 실제 픽셀 인덱스로 변환하고 경계 처리
                        //int src_x = static_cast<int>(p2.x * (float)screen_width);
                        //int src_y = static_cast<int>(p2.y * (float)screen_height);
                        // 좌표가 화면 범위 [0, width-1], [0, height-1] 내에 있도록 클램핑
                        //src_x = std::max(0, std::min(screen_width - 1, src_x));
                        //src_y = std::max(0, std::min(screen_height - 1, src_y));

                        //ONSBuf *temp_buffer = src_buffer + src_y * screen_width + src_x;
                        ONSBuf *temp_buffer = src_buffer + screen_width*((int)(p2.y*(float)screen_height)) + ((int)(p2.x*(float)screen_width));

                        for (int row = 0; row < blurN; ++row) {
                            for (int col = 0; col < blurN; ++col) {
                                    gfx2.imageFilterBlend(dst_buffer + row * screen_width + col, temp_buffer, alphap_base, alpha, 1);
                            }
                        }

                        pixel_processed = true;
                        break;
                    }
                }
            }
        }
    }

    SDL_UnlockSurface( accumulation_surface );
    SDL_UnlockSurface( effect_src_surface );
}