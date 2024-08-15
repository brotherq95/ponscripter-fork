/* -*- C++ -*-
 *
 *  PonscripterLabel_image.cpp - Image processing in Ponscripter
 *
 *  Copyright (c) 2001-2008 Ogapee (original ONScripter, of which this
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
#include <cstdio>

#include "graphics_common.h"

SDL_Surface *PonscripterLabel::loadImage(const pstring& filename,
                                        bool *has_alpha, bool twox, bool isflipped)
{
    if (!filename) return NULL;

    if (lastRenderEvent < RENDER_EVENT_LOAD_IMAGE) { lastRenderEvent = RENDER_EVENT_LOAD_IMAGE; }

    SDL_Surface *tmp = NULL, *tmpb = NULL;
    int location = BaseReader::ARCHIVE_TYPE_NONE;

    CBStringList filenames = filename.split("&", 4);

    if (filenames[0][0] == '>')
        tmp = createRectangleSurface(filenames[0]);
    else
        tmp = createSurfaceFromFile(filenames[0], &location);

    if (tmp == NULL) return NULL;

    bool has_colorkey = false;

    if ( has_alpha ){
        *has_alpha = (tmp->format->Amask != 0);
        if (!(*has_alpha) && (tmp->flags & SDL_TRUE)){
            has_colorkey = true;
            if (tmp->format->palette){
                //palette will be converted to RGBA, so don't do colorkey check
                has_colorkey = false;
            }
            *has_alpha = true;
        }
    }

    SDL_Surface *ret = SDL_ConvertSurface( tmp, image_surface->format, SDL_SWSURFACE );
    SDL_FreeSurface( tmp );

    SDL_Rect subimage_rect;
    CBStringList fileparts;
    pstring sub_filename;

    int num_images = filenames.size();
    if (num_images > 1) {
        for (int x = 1; x < num_images; x++) {
            sub_filename = filenames[x];
            fileparts = sub_filename.split(",", 3);
            tmp = createSurfaceFromFile(fileparts[2], &location);
            tmpb = SDL_ConvertSurface( tmp, image_surface->format, SDL_SWSURFACE );
            subimage_rect.x = fileparts[0];
            subimage_rect.y = fileparts[1];
            subimage_rect.w = tmpb->w;
            subimage_rect.h = tmpb->h;
            SDL_BlitScaled(tmpb, NULL, ret, &subimage_rect);
            SDL_FreeSurface( tmp );
            SDL_FreeSurface( tmpb );
        }
    }

    // Hack to detect when a PNG image is likely to have an old-style
    // mask.  We assume that an old-style mask is intended if the
    // image either has no alpha channel, or the alpha channel it has
    // is completely opaque.  This behaviour can be overridden with
    // the --force-png-alpha and --force-png-nscmask command-line
    // options.
    if (has_alpha && *has_alpha) {
        if (png_mask_type == PNG_MASK_USE_NSCRIPTER)
            *has_alpha = false;
        else if (png_mask_type == PNG_MASK_AUTODETECT) {
            SDL_LockSurface(ret);
            const Uint32 aval = *(Uint32*)ret->pixels & ret->format->Amask;
            if (aval != ret->format->Amask) goto breakalpha;
            *has_alpha = false;
            for (int y=0; y<ret->h; ++y) {
                Uint32* pixbuf = (Uint32*)((char*)ret->pixels + y * ret->pitch);
                for (int x=ret->w; x>0; --x, ++pixbuf) {
                    // Resolving ambiguity per Tatu's patch, 20081118.
                    // I note that this technically changes the meaning of the
                    // code, since != is higher-precedence than &, but this
                    // version is obviously what I intended when I wrote this.
                    // Has this been broken all along?  :/  -- Haeleth
                    if ((*pixbuf & ret->format->Amask) != aval) {
                        *has_alpha = true;
                        goto breakalpha;
                    }
                }
            }
          breakalpha:
            if (!*has_alpha && has_colorkey) {
                // has a colorkey, so run a match against rgb values
                const Uint32 aval = SDL_TRUE & ~(ret->format->Amask);
                if (aval == (*(Uint32*)ret->pixels & ~(ret->format->Amask)))
                    goto breakkey;
                *has_alpha = false;
                for (int y=0; y<ret->h; ++y) {
                    Uint32* pixbuf = (Uint32*)((char*)ret->pixels + y * ret->pitch);
                    for (int x=ret->w; x>0; --x, ++pixbuf) {
                        if ((*pixbuf & ~(ret->format->Amask)) == aval) {
                            *has_alpha = true;
                            goto breakkey;
                        }
                    }
                }
            }
          breakkey:
            SDL_UnlockSurface(ret);
        }
    }


    if (isflipped) {
        SDL_Surface *retf = SDL_CreateRGBSurface(0, ret->w, ret->h, BPP, RMASK, GMASK, BMASK, AMASK);
        Uint32* sourcepix;
        Uint32* destpix;
        for (int y=0; y<ret->h; ++y) {
            sourcepix = (Uint32*)((char*)ret->pixels + y * ret->pitch);
            destpix = (Uint32*)((char*)retf->pixels + (y + 1) * ret->pitch);
            destpix--;
            for (int x = 0; x < ret->w; x++, sourcepix++, destpix--) {
                *destpix = *sourcepix;
            }
        }
        // swap pointer to new surface
        SDL_FreeSurface( ret );
        ret = retf;
    }

    if (res_multiplier != 1) {
        int multiplier = twox ? 1 : res_multiplier;
        SDL_Surface *retb = SDL_CreateRGBSurface(0, ret->w * multiplier, ret->h * multiplier, BPP, RMASK, GMASK, BMASK, AMASK);
        SDL_BlitScaled(ret, NULL, retb, NULL);

        SDL_FreeSurface( ret );
        return retb;
    } else {
        return ret;
    }
    
}

SDL_Surface *PonscripterLabel::createRectangleSurface(const char* filename)
{
    int c=1, w=0, h=0;
    while (filename[c] != 0x0a && filename[c] != 0x00){
        if (filename[c] >= '0' && filename[c] <= '9')
            w = w*10 + filename[c]-'0';
        if (filename[c] == ','){
            c++;
            break;
        }
        c++;
    }

    while (filename[c] != 0x0a && filename[c] != 0x00){
        if (filename[c] >= '0' && filename[c] <= '9')
            h = h*10 + filename[c]-'0';
        if (filename[c] == ','){
            c++;
            break;
        }
        c++;
    }
        
    while (filename[c] == ' ' || filename[c] == '\t') c++;
    int n=0, c2 = c;
    while(filename[c] == '#'){
        //rgb_t col = readColour((const char *)filename + c);
        n++;
        c += 7;
        while (filename[c] == ' ' || filename[c] == '\t') c++;
    }

    SDL_PixelFormat *fmt = image_surface->format;
    SDL_Surface *tmp = SDL_CreateRGBSurface(0, w, h,
                                            fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);

    c = c2;
    for (int i=0 ; i<n ; i++){
        rgb_t col = readColour(filename + c);
        c += 7;
        while (filename[c] == ' ' || filename[c] == '\t') c++;
        
        SDL_Rect rect;
        rect.x = w*i/n;
        rect.y = 0;
        rect.w = w*(i+1)/n - rect.x;
        if (i == n-1) rect.w = w - rect.x;
        rect.h = h;
        SDL_FillRect(tmp, &rect, SDL_MapRGBA( accumulation_surface->format, col.r, col.g, col.b, 0xff));
    }
    
    return tmp;
}

SDL_Surface *PonscripterLabel::createSurfaceFromFile(const pstring& filename,
                                                    int *location)
{
    pstring alt_filename= "";
    unsigned long length = script_h.cBR->getFileLength( filename );

    if (length == 0) {
        alt_filename = script_h.save_path + filename;
        alt_filename.findreplace("\\", DELIMITER);
        FILE* fp = std::fopen(alt_filename, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            fclose(fp);
        }
    }

    if ( length == 0 ){
        //don't complain about missing cursors
        if ((filename != DEFAULT_LOOKBACK_NAME0) &&
            (filename != DEFAULT_LOOKBACK_NAME1) &&
            (filename != DEFAULT_LOOKBACK_NAME2) &&
            (filename != DEFAULT_LOOKBACK_NAME3) &&
            (filename != DEFAULT_CURSOR0) &&
            (filename != DEFAULT_CURSOR1))
            fprintf(stderr, " *** can't find file [%s] ***\n",
                    (const char*) filename);
        return NULL;
    }
    if (filelog_flag) script_h.file_log.add(filename);

    pstring dat = "";
    if (!alt_filename) {
        dat = script_h.cBR->getFile(filename, location);
    }
    else {
        dat = script_h.cBR->getFile(alt_filename, location);
        if ((unsigned)dat.slen != length)
            fprintf(stderr, "Warning: error reading from %s\n",
                    (const char*)alt_filename);
    }

    SDL_Surface* tmp = IMG_Load_RW(rwops(dat), 1);
    if (!tmp && file_extension(filename).caselessEqual("jpg")) {
        fprintf(stderr, " *** force-loading a JPEG image [%s]\n",
                (const char*) filename);
        SDL_RWops* src = rwops(dat);
        tmp = IMG_LoadJPG_RW(src);
        SDL_RWclose(src);
    }

    if (!tmp)
        fprintf(stderr, " *** can't load file [%s]: %s ***\n",
                (const char*)filename, IMG_GetError());

    return tmp;
}


// alphaMaskBlend
// dst: accumulation_surface
// src1: effect_src_surface
// src2: effect_dst_surface
void PonscripterLabel::alphaMaskBlend(SDL_Surface *mask_surface, int trans_mode,
                                      Uint32 mask_value, SDL_Rect *clip,
                                      SDL_Surface *src1, SDL_Surface *src2,
                                      SDL_Surface *dst)
{
    SDL_Rect rect = {0, 0, screen_width, screen_height};

    if (src1 == NULL)
        src1 = effect_src_surface;
    if (src2 == NULL)
        src2 = effect_dst_surface;
    if (dst == NULL)
        dst = accumulation_surface;

    /* ---------------------------------------- */
    /* clipping */
    if ( clip ){
        if ( AnimationInfo::doClipping( &rect, clip ) ) return;
    }

    /* ---------------------------------------- */

    SDL_LockSurface( src1 );
    SDL_LockSurface( src2 );
    SDL_LockSurface( dst );
    if ( mask_surface ) SDL_LockSurface( mask_surface );
    
    ONSBuf *src1_buffer = (ONSBuf *)src1->pixels + src1->w * rect.y + rect.x;
    ONSBuf *src2_buffer = (ONSBuf *)src2->pixels + src2->w * rect.y + rect.x;
    ONSBuf *dst_buffer  = (ONSBuf *)dst->pixels + dst->w * rect.y + rect.x;

    const int rwidth = screen_width - rect.w;
    SDL_PixelFormat *fmt = dst->format;
    Uint32 overflow_mask = 0xffffffff;
    if ( trans_mode != ALPHA_BLEND_FADE_MASK )
        overflow_mask = ~fmt->Bmask;

    mask_value >>= fmt->Bloss;

    if (( trans_mode == ALPHA_BLEND_FADE_MASK || trans_mode == ALPHA_BLEND_CROSSFADE_MASK ) && mask_surface) {
        bool accelerated_ok = sizeof(ONSBuf) == 4 && fmt->Bmask == 0xff;
        if (accelerated_ok) {
            bool ok = AnimationInfo::gfx.alphaMaskBlend(dst, src1, src2, mask_surface, rect, mask_value);
            accelerated_ok &= ok;
        }
        if (!accelerated_ok) {
            int mask_off_base_y = rect.y % mask_surface->h;
            int mask_off_base_x = rect.x % mask_surface->w;
            for ( int i=0, my=mask_off_base_y ; i<rect.h ; i++, my++ ) {
                if (my >= mask_surface->h) { my = 0; }
                ONSBuf *mask_buffer = (ONSBuf *)mask_surface->pixels + mask_surface->w * my;
                int offset=rect.x;
                for ( int j=rect.w, mx=mask_off_base_x ; j ; j--, mx++ ) {
                    if (mx >= mask_surface->w) { mx = 0; }
                    Uint32 mask2 = 0;
                    Uint32 mask = *(mask_buffer + mx) & fmt->Bmask;
                    if ( mask_value > mask ){
                        mask2 = mask_value - mask;
                        if ( mask2 & overflow_mask ) mask2 = fmt->Bmask;
                    }
    #ifndef BPP16
                    Uint32 mask1 = mask2 ^ fmt->Bmask;
    #endif
                    BLEND_MASK_PIXEL();
                    ++dst_buffer, ++src1_buffer, ++src2_buffer, ++offset;
                }
                src1_buffer += rwidth;
                src2_buffer += rwidth;
                dst_buffer  += rwidth;
            }
        }
    }
    else{ // ALPHA_BLEND_CONST
        if (sizeof(ONSBuf) == 4) {
            AnimationInfo::gfx.alphaMaskBlendConst(dst, src1, src2, rect, mask_value);
        }
        else {
            Uint32 mask2 = mask_value & fmt->Bmask;
#ifndef BPP16
            Uint32 mask1 = mask2 ^ fmt->Bmask;
#endif
            for ( int i=rect.h ; i ; i-- ) {
                for ( int j=rect.w ; j ; j-- ){
                    BLEND_MASK_PIXEL();
                    ++dst_buffer, ++src1_buffer, ++src2_buffer;
                }
                src1_buffer += rwidth;
                src2_buffer += rwidth;
                dst_buffer  += rwidth;
            }
        }
    }
    
    if ( mask_surface ) SDL_UnlockSurface( mask_surface );
    SDL_UnlockSurface( dst );
    SDL_UnlockSurface( src2 );
    SDL_UnlockSurface( src1 );
}


// alphaBlendText
// dst: ONSBuf surface (accumulation_surface)
// txt: 8bit surface (TTF_RenderGlyph_Shaded())
void PonscripterLabel::alphaBlendText(SDL_Surface *dst_surface, SDL_Rect dst_rect,
                                      SDL_Surface *txt_surface, SDL_Color &color,
                                      SDL_Rect *clip, bool rotate_flag)
{
    int x2=0, y2=0;
    SDL_Rect clipped_rect;

    /* ---------------------------------------- */
    /* 1st clipping */
    if ( clip ){
        if ( AnimationInfo::doClipping( &dst_rect, clip, &clipped_rect ) ) return;

        x2 += clipped_rect.x;
        y2 += clipped_rect.y;
    }

    /* ---------------------------------------- */
    /* 2nd clipping */
    SDL_Rect clip_rect = {0, 0, dst_surface->w, dst_surface->h};
    if ( AnimationInfo::doClipping( &dst_rect, &clip_rect, &clipped_rect ) ) return;
    
    x2 += clipped_rect.x;
    y2 += clipped_rect.y;

    /* ---------------------------------------- */

    SDL_LockSurface( dst_surface );
    SDL_LockSurface( txt_surface );

#ifdef BPP16
    Uint32 src_color = ((color.r >> RLOSS) << RSHIFT) |
                       ((color.g >> GLOSS) << GSHIFT) |
                       (color.b >> BLOSS);
    src_color = (src_color | src_color << 16) & BLENDMASK;
#else
    Uint32 src_color1 = color.r << RSHIFT | color.b;
    Uint32 src_color2 = color.g << GSHIFT;
#endif

    ONSBuf *dst_buffer = (ONSBuf *)dst_surface->pixels +
                         dst_surface->w * dst_rect.y + dst_rect.x;

    if (!rotate_flag){
        unsigned char *src_buffer = (unsigned char*)txt_surface->pixels +
                                    txt_surface->pitch * y2 + x2;
        for ( int i=dst_rect.h ; i>0 ; i-- ){
            for ( int j=dst_rect.w ; j>0 ; j--, dst_buffer++, src_buffer++ ){
                BLEND_PIXEL8();
            }
            dst_buffer += dst_surface->w - dst_rect.w;
            src_buffer += txt_surface->pitch - dst_rect.w;
        }
    }
    else{
        for ( int i=0 ; i<dst_rect.h ; i++ ){
            unsigned char *src_buffer = (unsigned char*)txt_surface->pixels + txt_surface->pitch*(txt_surface->h - x2 - 1) + y2 + i;
            for ( int j=dst_rect.w ; j>0 ; j--, dst_buffer++ ){
                BLEND_PIXEL8();
                src_buffer -= txt_surface->pitch;
            }
            dst_buffer += dst_surface->w - dst_rect.w;
        }
    }
    
    SDL_UnlockSurface( txt_surface );
    SDL_UnlockSurface( dst_surface );
}


void PonscripterLabel::makeNegaSurface( SDL_Surface *surface, SDL_Rect &clip )
{
    SDL_LockSurface( surface );
    ONSBuf *buf = (ONSBuf *)surface->pixels + clip.y * surface->w + clip.x;

    ONSBuf mask = surface->format->Rmask | surface->format->Gmask | surface->format->Bmask;
    for ( int i=clip.h ; i>0 ; i-- ){
        for ( int j=clip.w ; j>0 ; j-- )
            *buf++ ^= mask;
        buf += surface->w - clip.w;
    }

    SDL_UnlockSurface( surface );
}


void PonscripterLabel::makeMonochromeSurface( SDL_Surface *surface, SDL_Rect &clip )
{
    SDL_LockSurface( surface );
    ONSBuf *buffer = (ONSBuf *)surface->pixels + clip.y * surface->w + clip.x;

    for ( int i=clip.h ; i>0 ; i-- ){
        for ( int j=clip.w ; j>0 ; j--, buffer++ ){
            //Mion: NScr seems to use more "equal" 85/86/85 RGB blending, instead
            // of the 77/151/28 that onscr used to have. Using 85/86/85 now,
            // might add a parameter to "monocro" to allow choosing 77/151/28
            MONOCRO_PIXEL();
        }
        buffer += surface->w - clip.w;
    }

    SDL_UnlockSurface( surface );
}

bool
PonscripterLabel::simul_checkFlushSub(){
    bool check_flag = false;
    bool channel_flag;
    effect_flag = false;
    int i, j, temp_infoIndex, temp_period;
    Uint32 now = SDL_GetTicks();
    for(i=MAX_CHANNEL_NUM-1; i>=0; i--){
        if(simul_Channel[i].mode){
            if(lipall_flag) dirty_rect.fill(screen_width, screen_height);
            channel_flag = false;
            if(simul_Channel[i].visible_with_effect) effect_flag = true;
            if(simul_Channel[i].mode == 10 && simul_rainNum){
                if(now >= simul_rainstartTick + simul_rainPeriod){
                    simul_rainstartTick = now;
                    simul_drawRain();
                    Uint32 after_rain = SDL_GetTicks();
                    if(after_rain - now + 5 > simul_rainPeriod)
                        simul_rainPeriod += 16; //prevent lag
                    dirty_rect.fill(screen_width, screen_height);
                    check_flag = true;
                }
                continue;
            }
            else if(simul_Channel[i].mode == 11){
                if(now >= simul_shakestartTick + simul_shakePeriod){
                    simul_shakemode = true;
                    simul_shakestartTick = now;
                    double tempx, tempy;
                    tempx = 10 * SDL_sin(2*M_PI*((double) now)/((double) simul_shakePx));
                    tempy = 40 + 40 * SDL_cos(2*M_PI*((double) now)/((double) simul_shakePy));
                    simul_shakex = (int) tempx;
                    simul_shakey = (int) tempy;
                    dirty_rect.fill(screen_width, screen_height);
                    check_flag = true;
                }
            }
            if(simul_Channel[i].mode > 2 || simul_Channel[i].mode <= 0)
                continue;
            if(simul_Channel[i].mode == 1 && simul_Channel[i].fade_time){
                if(now < simul_Channel[i].startTick + simul_Channel[i].fade_time){
                    channel_flag = true;
                    dirty_rect.fill(screen_width, screen_height);
                    simul_info[simul_Channel[i].infoIndex[0]].trans = simul_Channel[i].fade_trans * (now - simul_Channel[i].startTick) / (simul_Channel[i].fade_time);
                }
                else if(now > simul_Channel[i].timestamp[1] - simul_Channel[i].fade_time){
                    channel_flag = true;
                    dirty_rect.fill(screen_width, screen_height);
                    simul_info[simul_Channel[i].infoIndex[0]].trans = simul_Channel[i].fade_trans * (simul_Channel[i].timestamp[1] - now) / (simul_Channel[i].fade_time);
                }
                else{
                    channel_flag = true;
                    dirty_rect.fill(screen_width, screen_height);
                    simul_info[simul_Channel[i].infoIndex[0]].trans = simul_Channel[i].fade_trans;
                }
            }
            while(now >= simul_Channel[i].timestamp[1+simul_Channel[i].current]){
                channel_flag = true;
                if(simul_Channel[i].current != -1){
                    temp_infoIndex = simul_Channel[i].infoIndex[simul_Channel[i].current];
                    if(temp_infoIndex < 0 || temp_infoIndex >= MAX_SIMUL_NUM + MAX_LIP_NUM*3){
                        channel_flag = false;
                        simul_Channel[i].mode = 0;
                        int ii;
                        for(j=0; j<simul_Channel[i].stampNum; j++){
                            ii = simul_Channel[i].infoIndex[j];
                            if(ii >=0 && ii < MAX_SIMUL_NUM){
                                simul_info[ii].remove();
                            }
                        }
                        break;
                    }
                    else if(temp_infoIndex < MAX_SIMUL_NUM){
                        dirty_rect.fill(screen_width, screen_height);//dirty_rect.add(simul_info[temp_infoIndex].pos);
                    }
                    else{
                        temp_infoIndex -= MAX_SIMUL_NUM;
                        if(simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].affine_flag)
                            dirty_rect.add(simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].bounding_rect);
                        else
                            dirty_rect.add(simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].pos);
                    }
                }
                simul_Channel[i].current += 1;
                if(simul_Channel[i].current == simul_Channel[i].stampNum - 1){
                    if(simul_Channel[i].mode == 2){
                        simul_Channel[i].current = -1;
                        simul_Channel[i].need_load = false;
                        temp_period = simul_Channel[i].timestamp[simul_Channel[i].stampNum - 1] - simul_Channel[i].startTick;
                        for(j=0; j<simul_Channel[i].stampNum; j++){
                            simul_Channel[i].timestamp[j] += temp_period;
                        }
                        continue;
                    }
                    channel_flag = false;
                    check_flag = true;
                    simul_Channel[i].mode = 0;
                    int ii;
                    for(j=0; j<simul_Channel[i].stampNum; j++){
                        ii = simul_Channel[i].infoIndex[j];
                        if(ii >=0 && ii < MAX_SIMUL_NUM){
                            simul_info[ii].remove();
                        }
                    }
                    break;
                }
                if(simul_Channel[i].need_load){
                    temp_infoIndex = simul_Channel[i].infoIndex[simul_Channel[i].current];
                    if(temp_infoIndex < MAX_SIMUL_NUM)
                        setupAnimationInfo(&simul_info[temp_infoIndex]);
                }
            }
            if(channel_flag){
                check_flag = true;
                if(simul_Channel[i].current != -1){
                    temp_infoIndex = simul_Channel[i].infoIndex[simul_Channel[i].current];
                    if(temp_infoIndex < 0 || temp_infoIndex >= MAX_SIMUL_NUM + MAX_LIP_NUM*3){
                        continue;
                    }
                    else if(temp_infoIndex < MAX_SIMUL_NUM){
                        if(simul_info[temp_infoIndex].affine_flag)
                            dirty_rect.add(simul_info[temp_infoIndex].bounding_rect);
                        else
                            dirty_rect.add(simul_info[temp_infoIndex].pos);
                    }
                    else{
                        temp_infoIndex -= MAX_SIMUL_NUM;
                        if(simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].affine_flag)
                            dirty_rect.add(simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].bounding_rect);
                        else
                            dirty_rect.add(simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].pos);
                    }
                }
            }
        }
    }
    return check_flag;
}

void
PonscripterLabel::simul_checkFlush()
{
    if(pause_flag) return;
    bool check_flag;
    check_flag = simul_checkFlushSub();
    if(!doing_effect){
        if(check_flag){
            if(skip_flag || ctrl_pressed_status || skip_to_wait){
                simul_Channel[0].mode = 0;
                return;
            }
            from_simul = true;
            flush(refreshMode());
            from_simul = false;
        }
        /*if(effect_flag){
            SDL_Rect temp_rect={0, 0, screen_width, screen_height};
            bool temp_flag = setEffect_flag;
            setEffect_flag = true;
            refreshSurface(simul_src_surface, &temp_rect, refreshMode());
            setEffect_flag = temp_flag;
        }*/
    }
}

void
PonscripterLabel::simul_drawRain()
{
    if(rainpause_flag < 0) return;
    if(rainpause_flag) rainpause_flag = -1;
    SDL_FillRect(simul_rain_surface, NULL, SDL_MapRGBA(simul_rain_surface->format, 0, 0, 0, 0));
    int max_length = (screen_height * simul_rainMaxLength) / 100;
    int min_length = (screen_height * simul_rainMinLength) / 100;

    SDL_Rect temp_rect={0, 0, screen_width, screen_height};
    for(int i=0; i<simul_rainNum; i++){
        drop_info.pos.x = rand() % screen_width; 
        drop_info.pos.y = (rand() % (screen_height + max_length)) - max_length;
        drop_info.scale_y = (min_length*100 + (max_length - min_length)*(rand() % 1000)/10)/drop_info.pos.h;
        
        drop_info.calcAffineMatrix();
        drawTaggedSurface(simul_rain_surface, &drop_info, temp_rect);
    }
}


void
PonscripterLabel::drawTaggedSurface2(SDL_Surface* dst_surface, AnimationInfo* anim, SDL_Rect &clip){
    if(!pause_flag && simul_shakemode){
        anim->pos.x += simul_shakex;
        anim->pos.y += simul_shakey;
    }

    drawTaggedSurface(dst_surface, anim, clip);

    if(!pause_flag && simul_shakemode){
        anim->pos.x -= simul_shakex;
        anim->pos.y -= simul_shakey;
    }
}

void
PonscripterLabel::simul_refreshSub(SDL_Surface* surface, SDL_Rect &clip, int priority)
{
    if(pause_flag || skip_flag || ctrl_pressed_status || skip_to_wait){
        simul_Channel[0].mode = 0;
        return;
    }
    //if(setEffect_flag) return;
    int temp_infoIndex;
    for(int i=0; i<MAX_CHANNEL_NUM; i++){
        if(simul_Channel[i].mode && (simul_Channel[i].priority == priority || (doing_effect && priority == -1 && simul_Channel[i].visible_with_effect))){
            if(simul_Channel[i].mode == 10){
                SDL_BlitSurface( simul_rain_surface, &clip, surface, &clip );
                continue;
            }
            if(simul_Channel[i].mode > 2 || simul_Channel[i].mode <= 0)
                continue;
            if(simul_Channel[i].current == -1)
                continue;
            if(setEffect_flag && simul_Channel[i].visible_with_effect)
                continue;
            temp_infoIndex = simul_Channel[i].infoIndex[simul_Channel[i].current];
            if(temp_infoIndex < 0)
                continue;
            if(temp_infoIndex < MAX_SIMUL_NUM){
                if (simul_info[temp_infoIndex].image_surface){
                    if(priority < 3)
                        drawTaggedSurface(surface, &simul_info[temp_infoIndex], clip);
                    else
                        drawTaggedSurface2(surface, &simul_info[temp_infoIndex], clip);
                }
            }
            else{
                temp_infoIndex -= MAX_SIMUL_NUM;
                if(temp_infoIndex >= MAX_LIP_NUM*3)
                    continue;
                if (simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].image_surface && simul_lip_info[temp_infoIndex/3][temp_infoIndex%3].showing()){
                    if(priority < 3)
                        drawTaggedSurface(surface, &simul_lip_info[temp_infoIndex/3][temp_infoIndex%3], clip);
                    else
                        drawTaggedSurface2(surface, &simul_lip_info[temp_infoIndex/3][temp_infoIndex%3], clip);
                }
            }
        }
    }
    if(lipall_flag && priority==3){
        if(simul_Channel[0].mode){
            temp_infoIndex = simul_Channel[0].infoIndex[simul_Channel[0].current];
            temp_infoIndex -= MAX_SIMUL_NUM;
            if(temp_infoIndex >= MAX_LIP_NUM*3) return;
            for(int lii = 0; lii<MAX_LIP_NUM; lii++){
                if(lii == temp_infoIndex/3) continue;
                if(simul_lip_id[lii] == simul_lip_id[temp_infoIndex/3]){
                    if (simul_lip_info[lii][temp_infoIndex%3].image_surface && simul_lip_info[lii][temp_infoIndex%3].showing()){
                        drawTaggedSurface2(surface, &simul_lip_info[lii][temp_infoIndex%3], clip);
                    }
                }
            }
        }
    }
}

void
PonscripterLabel::refreshSurface(SDL_Surface* surface, SDL_Rect* clip_src,
				 int refresh_mode)
{
    if (refresh_mode == REFRESH_NONE_MODE) return;

    if(!from_simul){
        simul_checkFlushSub();
    }
        if (surface == accumulation_surface){
            SDL_Rect temp_rect={0, 0, screen_width, screen_height};
            bool temp_flag = setEffect_flag;
            setEffect_flag = true;
            refreshSurface(simul_src_surface, &temp_rect, refresh_mode);
            setEffect_flag = temp_flag;
        }
    //}

    SDL_Rect clip = { 0, 0, surface->w, surface->h };
    if (clip_src && AnimationInfo::doClipping(&clip, clip_src)) return;

    if(shake_now>0){
        clip = {0, 0, screen_width, screen_height};
    }

    int i, top;
    SDL_BlitSurface( bg_info.image_surface, &clip, surface, &clip );

    simul_refreshSub(surface, clip, 9);

    simul_refreshSub(surface, clip, 8);

    if (!all_sprite_hide_flag) {
        if (z_order < 10 && refresh_mode & REFRESH_SAYA_MODE)
            top = 9;
        else
            top = z_order;
    
        for (i = MAX_SPRITE_NUM - 1; i > top; --i) {
            if(i==601) continue;
            if(i==602) continue;
            if (sprite_info[i].image_surface && sprite_info[i].showing()){
                if(604<=i && i<=607)
                    drawTaggedSurface(surface, &sprite_info[i], clip);
                else
                    drawTaggedSurface2(surface, &sprite_info[i], clip);
                if(i == simul_voice_sno) simul_refreshSub(surface, clip, 11);
            }
        }
    }

    simul_refreshSub(surface, clip, 7);

    for (i = 0; i < 3; ++i) {
        if (human_order[2 - i] >= 0 &&
            tachi_info[human_order[2 - i]].image_surface)
            drawTaggedSurface2(surface, &tachi_info[human_order[2 - i]], clip);
    }

    simul_refreshSub(surface, clip, 6);
    
    if (windowback_flag) {
        if (nega_mode == 1) makeNegaSurface(surface, clip);
        if (monocro_flag)   makeMonochromeSurface(surface, clip);
        if (nega_mode == 2) makeNegaSurface(surface, clip);

        if (!all_sprite2_hide_flag) {
            for (i = MAX_SPRITE2_NUM - 1; i >= 0; --i) {
                if (sprite2_info[i].image_surface && sprite2_info[i].showing()){
                    drawTaggedSurface2(surface, &sprite2_info[i], clip);
                    if(i == simul_voice_sno-10000) simul_refreshSub(surface, clip, 11);
                }
            }
        }

        if (refresh_mode & REFRESH_SHADOW_MODE)
            shadowTextDisplay(surface, clip);
        if (refresh_mode & REFRESH_TEXT_MODE)
            text_info.blendOnSurface(surface, 0, 0, clip);
    }

    simul_refreshSub(surface, clip, 5);

    if (!all_sprite_hide_flag) {
        if (refresh_mode & REFRESH_SAYA_MODE)
            top = 10;
        else
            top = 0;
        for (i = z_order; i >= top; --i) {
            if(i==601) continue;
            if(i==602) continue;
            if (sprite_info[i].image_surface && sprite_info[i].showing()){
                if(604<=i && i<=607)
                    drawTaggedSurface(surface, &sprite_info[i], clip);
                else
                    drawTaggedSurface2(surface, &sprite_info[i], clip);
                if(i == simul_voice_sno) simul_refreshSub(surface, clip, 11);
            }
        }
    }

    simul_refreshSub(surface, clip, 4);

    if (!windowback_flag) {
        //Mion - ogapee2008
        if (!all_sprite2_hide_flag) {
            for (i = MAX_SPRITE2_NUM - 1; i >= 0; --i) {
                if (sprite2_info[i].image_surface && sprite2_info[i].showing()){
                    drawTaggedSurface2(surface, &sprite2_info[i], clip);
                    if(i == simul_voice_sno-10000) simul_refreshSub(surface, clip, 11);
                }
            }
        }
        if (nega_mode == 1) makeNegaSurface(surface, clip);
        if (monocro_flag)   makeMonochromeSurface(surface, clip);
        if (nega_mode == 2) makeNegaSurface(surface, clip);
    }
    
    simul_refreshSub(surface, clip, 3);

    if (!(refresh_mode & REFRESH_SAYA_MODE)) {
        for (i = 0; i < MAX_PARAM_NUM; ++i)
            if (bar_info[i])
                drawTaggedSurface(surface, bar_info[i], clip);
        for (i = 0; i < MAX_PARAM_NUM; ++i)
            if (prnum_info[i])
                drawTaggedSurface(surface, prnum_info[i], clip);
    }

    if (!windowback_flag) {
        if (refresh_mode & REFRESH_SHADOW_MODE)
            shadowTextDisplay(surface, clip);
        if (refresh_mode & REFRESH_TEXT_MODE)
            text_info.blendOnSurface(surface, 0, 0, clip);
    }

    if (refresh_mode & REFRESH_CURSOR_MODE && !textgosub_label) {
        if (clickstr_state == CLICK_WAIT)
            drawTaggedSurface(surface, &cursor_info[CURSOR_WAIT_NO], clip);
        else if (clickstr_state == CLICK_NEWPAGE)
            drawTaggedSurface(surface, &cursor_info[CURSOR_NEWPAGE_NO], clip);
    }

    i=601; //force cursor TextCC1SpNum 601 TextCC2SpNum 602
    if (sprite_info[i].image_surface && sprite_info[i].showing())
        drawTaggedSurface(surface, &sprite_info[i], clip);
    i=602;
    if (sprite_info[i].image_surface && sprite_info[i].showing())
        drawTaggedSurface(surface, &sprite_info[i], clip);

    simul_refreshSub(surface, clip, 2);

    for (ButtonElt::iterator it = buttons.begin(); it != buttons.end(); ++it)
        if (it->second.show_flag > 0)
            drawTaggedSurface(surface, it->second.anim[it->second.show_flag - 1], clip);

    simul_refreshSub(surface, clip, 1);
    simul_refreshSub(surface, clip, -1);


    clip = {screen_width - 90, 20, 90, 100};
    if(skip_flag || ctrl_pressed_status)
        drawTaggedSurface(surface, &skip_info, clip);
    else if(automode_flag)
        drawTaggedSurface(surface, &auto_info, clip);
    else{
        skip_info.trans = 0;
        drawTaggedSurface(surface, &skip_info, clip);
        skip_info.trans = 256;
    }
}


void PonscripterLabel::refreshSprite(int sprite_no, bool active_flag,
                                     int cell_no, SDL_Rect* check_src_rect,
                                     SDL_Rect* check_dst_rect)
{
    if ((sprite_info[sprite_no].image_name ||
         ((sprite_info[sprite_no].trans_mode == AnimationInfo::TRANS_STRING) &&
          sprite_info[sprite_no].file_name)) &&
        ((sprite_info[sprite_no].showing() != active_flag) ||
         (cell_no >= 0 && sprite_info[sprite_no].current_cell != cell_no) ||
         AnimationInfo::doClipping(check_src_rect,&sprite_info[sprite_no].pos) == 0 ||
         AnimationInfo::doClipping(check_dst_rect,&sprite_info[sprite_no].pos) == 0))
    {
        if (cell_no >= 0) sprite_info[sprite_no].setCell(cell_no);
        sprite_info[sprite_no].visible(active_flag);
        dirty_rect.add(sprite_info[sprite_no].pos);
    }
}


void PonscripterLabel::createBackground()
{
    bg_info.num_of_cells = 1;
    bg_info.trans_mode = AnimationInfo::TRANS_COPY;
    bg_info.pos.x = 0;
    bg_info.pos.y = 0;
    bg_info.allocImage(screen_width,screen_height);

    if (bg_info.file_name == "white") {
        bg_info.color.set(0xff);
    }
    else if (bg_info.file_name == "black") {
        bg_info.color.set(0x00);
    }
    else if (bg_info.file_name[0] == '#') {
        bg_info.color = readColour(bg_info.file_name);
    }
    else {
        AnimationInfo anim;
        anim.image_name = bg_info.file_name;
        parseTaggedString(&anim);
        anim.trans_mode = AnimationInfo::TRANS_COPY;
        setupAnimationInfo(&anim);
        bg_info.fill(0, 0, 0, 0xff);
        if (anim.image_surface){
            SDL_Rect src_rect = {0, 0, anim.image_surface->w, anim.image_surface->h};
            SDL_Rect dst_rect = {0, 0, screen_width, screen_height};
            if (screen_width >= anim.image_surface->w){
                dst_rect.x = (screen_width - anim.image_surface->w) / 2;
            }
            else{
                src_rect.x = (anim.image_surface->w - screen_width) / 2;
                src_rect.w = screen_width;
            }
            if (screen_height >= anim.image_surface->h){
                dst_rect.y = (screen_height - anim.image_surface->h) / 2;
            }
            else{
                src_rect.y = (anim.image_surface->h - screen_height) / 2;
                src_rect.h = screen_height;
            }
            bg_info.copySurface(anim.image_surface, &src_rect, &dst_rect);
        }
        return;
    }

    bg_info.fill(bg_info.color, 0xff);
}
