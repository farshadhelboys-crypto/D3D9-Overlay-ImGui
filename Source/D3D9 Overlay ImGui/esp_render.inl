void RenderESPMenu(){
    if(!UI::IsWindowTargeted())return; EnsureOffsets();
    ImGui::SetNextWindowSize(ImVec2(360,440),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("farshad pm Esp",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysVerticalScrollbar)){ImGui::End();return;}
    ImGui::TextColored(ImVec4(0.4f,0.85f,1.f,1.f),"farshad pm Esp"); ImGui::SameLine(); ImGui::TextDisabled("esp demo");
    ImGui::Separator();
    ImGui::Checkbox("Enable ESP",&cfg.enabled); ImGui::SameLine();
    if(ImGui::Button("Attach")) AttachFromUI();
    ImGui::SameLine(); ImGui::TextDisabled("F5 = Attach/Rescan");
    if(ImGui::Button("Reload config")){g_offsetsTried=false;Off.loaded=false;EnsureOffsets();}
    ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f),"%s",g_status);
    if(!Off.loaded) ImGui::TextDisabled("%s",Off.status);
    ImGui::Separator();
    if(ImGui::BeginTabBar("tabs")){
        if(ImGui::BeginTabItem("Main")){ImGui::Checkbox("Team Check",&cfg.teamCheck);ImGui::SliderFloat("Max Distance",&cfg.maxDistance,50.f,4000.f,"%.0f m");ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Visuals")){
            ImGui::Checkbox("Box",&cfg.drawBox);ImGui::SameLine();ImGui::Checkbox("Corners",&cfg.drawCorners);
            ImGui::Checkbox("Health Bar",&cfg.drawHealth);ImGui::SameLine();ImGui::Checkbox("Distance",&cfg.drawDistance);
            ImGui::SliderFloat("Thickness",&cfg.thickness,0.5f,6.f,"%.1f");
            ImGui::SliderFloat("Box Height",&cfg.headOffset,30.f,140.f,"%.0f");
            ImGui::SliderFloat("Width Ratio",&cfg.widthRatio,0.15f,0.9f,"%.2f");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Aimbot")){
            ImGui::Checkbox("Enable Aim Assist",&cfg.aimbot);
            ImGui::Separator();
            ImGui::TextDisabled("Activation");
            ImGui::Checkbox("Aim on RMB (right click)",&cfg.aimOnRMB);
            ImGui::Checkbox("Aim on LMB (left click)",&cfg.aimOnLMB);
            ImGui::Checkbox("Aim on ALT (pull + autofire)",&cfg.aimOnAlt);
            ImGui::Checkbox("Aim on ALT (aim only, no shoot)",&cfg.aimOnAltAimOnly);
            ImGui::Separator();
            ImGui::SliderFloat("Smooth (RMB/LMB)",&cfg.aimSmooth,1.f,20.f,"%.1f");
            ImGui::SliderFloat("ALT Smooth",&cfg.aimAltSmooth,1.f,25.f,"%.1f");
            ImGui::SliderFloat("ALT Precision (px)",&cfg.aimAltPrecision,1.f,40.f,"%.0f");
            ImGui::SliderFloat("Fire Delay (ms)",&cfg.aimFireDelayMs,50.f,1000.f,"%.0f");
            ImGui::SliderFloat("FOV (px)",&cfg.aimFov,0.f,600.f,"%.0f");
            ImGui::TextDisabled("ALT aim-only = pull only; Fire Delay = autofire rate");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Status")){
            ImGui::Text("Connected: %s",g_hProc?"Yes":"No");
            ImGui::Text("Local: %s",g_haveLocal?"Found":"Not found");
            ImGui::Text("Camera: %s",g_dbgVmOk?"OK":"Failed");
            ImGui::Text("Players: %d  Drawn: %d",g_dbgCount,g_dbgDrawn);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Contact")){
            ImGui::Text("Telegram: @client_v34");
            if(ImGui::Button("Open Telegram",ImVec2(-1,0))) ShellExecuteA(nullptr,"open","https://t.me/client_v34",nullptr,nullptr,SW_SHOWNORMAL);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void RenderESPBoxes(){
    EnsureOffsets(); if(!Off.loaded) return;
    if (GetAsyncKeyState(VK_F5) & 1) {
        if (UI::IsWindowTargeted()) AttachFromUI();
    }
    if(UI::GetTargetWindow()!=g_lastTarget){
        g_lastTarget=UI::GetTargetWindow();
        Detach();
    }
    if(!g_hProc||!g_gameBase) return;
    UpdateLocal(); if(!cfg.enabled||!g_scanned) return;

    ImGuiIO& io=ImGui::GetIO(); const float sw=io.DisplaySize.x, sh=io.DisplaySize.y;
    ImDrawList* dl=ImGui::GetBackgroundDrawList(); if(!dl) return;
    float mat[16]={}; g_dbgVmOk=RPM(g_gameBase+Off.MATRIX_RVA,mat);
    if(!g_dbgVmOk){ dl->AddText(ImVec2(20,70),IM_COL32(255,80,80,255),"Camera read failed"); return; }

    g_dbgDrawn=0; g_dbgCount=0;
    float bestD=1e9f,bestX=0,bestY=0; bool hasAim=false;

    for(auto& te:g_tracked){
        Vec3 origin{};
        bool ok=RPM(te.obj+Off.V_OBJ_ORIGIN,origin.x)&&RPM(te.obj+Off.V_OBJ_ORIGIN+4,origin.y)&&RPM(te.obj+Off.V_OBJ_ORIGIN+8,origin.z);
        if(ok){
            if(fabsf(origin.x)>30000.f||fabsf(origin.y)>30000.f||fabsf(origin.z)>30000.f) ok=false;
            if(fabsf(origin.x)<0.05f&&fabsf(origin.y)<0.05f) ok=false;
        }
        int hp=0,team=0,lifeState=0;
        if(ok){
            RPM(te.obj+Off.OBJ_HP_OFF,hp);
            RPM(te.obj+Off.OBJ_TEAM_OFF,team);
            if(Off.OBJ_LIFE_OFF) RPM(te.obj+Off.OBJ_LIFE_OFF,lifeState);
            if(hp<=0 || lifeState!=0){
                te.hp=0; te.valid=false; te.life=0;
            } else {
                te.hp=hp; te.team=team;
                te.pos=origin;
                te.life=kMaxLife;
                te.valid=true;
            }
        } else {
            if(te.life>0) te.life--;
            if(te.life<=0) te.valid=false;
        }
        if(!te.valid) continue;
        if(te.hp<=0) continue;
        if(cfg.teamCheck&&g_localTeam&&te.team==g_localTeam) continue;
        float dist=-1.f;
        if(g_haveLocal){
            float dx=te.pos.x-g_localPos.x,dy=te.pos.y-g_localPos.y,dz=te.pos.z-g_localPos.z;
            dist=sqrtf(dx*dx+dy*dy+dz*dz);
            if(dist<15.f||dist>cfg.maxDistance) continue;
        }
        ++g_dbgCount;
        Vec3 head=te.pos; head.z+=cfg.headOffset;
        Vec2 feet{},headS{};
        if(!WorldToScreen(te.pos,mat,sw,sh,feet)) continue;
        if(!WorldToScreen(head,mat,sw,sh,headS)) continue;
        float h=feet.y-headS.y; if(h<6.f||h>2000.f) continue;
        float w=h*cfg.widthRatio; if(w<2.f)w=2.f;
        float bx=headS.x-w*0.5f, by=headS.y;
        ImU32 col=C(cfg.enemyColor);
        if(cfg.drawBox) dl->AddRect(ImVec2(bx,by),ImVec2(bx+w,by+h),col,0.f,0,cfg.thickness);
        if(cfg.drawDistance&&dist>=0.f){ char b[32]; sprintf_s(b,"%.0fm",dist); ImVec2 ts=ImGui::CalcTextSize(b); dl->AddText(ImVec2(bx+w*0.5f-ts.x*0.5f,by+h+2.f),IM_COL32(255,255,255,230),b); }
        ++g_dbgDrawn;
        if(cfg.aimbot){
            float ax=headS.x, ay=headS.y+h*0.25f;
            float adx=ax-sw*0.5f, ady=ay-sh*0.5f; float ad=sqrtf(adx*adx+ady*ady);
            float fovLimit = cfg.aimFov <= 0.f ? 1.f : cfg.aimFov;
            if(ad<bestD&&ad<=fovLimit){bestD=ad;bestX=ax;bestY=ay;hasAim=true;}
        }
    }

    if(cfg.aimbot && cfg.aimFov > 0.f)
        dl->AddCircle(ImVec2(sw*0.5f,sh*0.5f),cfg.aimFov,IM_COL32(255,255,255,40),64,1.f);

    if(hasAim && cfg.aimbot){
        const bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        const bool useRmb = cfg.aimOnRMB && rmb;
        const bool useLmb = cfg.aimOnLMB && lmb;
        const bool useAltFire = cfg.aimOnAlt && alt;
        const bool useAltOnly = cfg.aimOnAltAimOnly && alt;
        const bool useAlt = useAltFire || useAltOnly;
        const bool anyMode = cfg.aimOnRMB || cfg.aimOnLMB || cfg.aimOnAlt || cfg.aimOnAltAimOnly;
        const bool active = useRmb || useLmb || useAlt || !anyMode;

        if(active){
            float cx = sw * 0.5f, cy = sh * 0.5f;
            float dx = bestX - cx, dy = bestY - cy;
            float dist = sqrtf(dx*dx + dy*dy);

            dl->AddLine(ImVec2(bestX-8,bestY),ImVec2(bestX+8,bestY),IM_COL32(0,255,0,230),1.5f);
            dl->AddLine(ImVec2(bestX,bestY-8),ImVec2(bestX,bestY+8),IM_COL32(0,255,0,230),1.5f);
            dl->AddLine(ImVec2(cx,cy),ImVec2(bestX,bestY),IM_COL32(0,255,0,120),1.0f);

            float sm = useAlt ? cfg.aimAltSmooth : cfg.aimSmooth;
            if(sm < 1.f) sm = 1.f;
            float stepX = dx / sm, stepY = dy / sm;
            const float maxStep = 45.f;
            if(stepX >  maxStep) stepX =  maxStep;
            if(stepX < -maxStep) stepX = -maxStep;
            if(stepY >  maxStep) stepY =  maxStep;
            if(stepY < -maxStep) stepY = -maxStep;

            int mx = (int)(stepX >= 0 ? stepX + 0.5f : stepX - 0.5f);
            int my = (int)(stepY >= 0 ? stepY + 0.5f : stepY - 0.5f);
            if(!mx && fabsf(dx) > 1.5f) mx = dx > 0 ? 1 : -1;
            if(!my && fabsf(dy) > 1.5f) my = dy > 0 ? 1 : -1;

            if(mx || my){
                INPUT in{}; in.type=INPUT_MOUSE;
                in.mi.dx=mx; in.mi.dy=my; in.mi.dwFlags=MOUSEEVENTF_MOVE;
                SendInput(1,&in,sizeof(INPUT));
            }

            if(useAltFire){
                float prec = cfg.aimAltPrecision < 1.f ? 1.f : cfg.aimAltPrecision;
                if(dist <= prec){
                    DWORD now = GetTickCount();
                    DWORD delay = (DWORD)(cfg.aimFireDelayMs < 50.f ? 50.f : cfg.aimFireDelayMs);
                    if(now - g_lastAutoShot > delay){
                        g_lastAutoShot = now;
                        INPUT click[2]{};
                        click[0].type = INPUT_MOUSE;
                        click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                        click[1].type = INPUT_MOUSE;
                        click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(2, click, sizeof(INPUT));
                    }
                }
            }
        }
    }
}

void RenderESP(){ RenderESPMenu(); RenderESPBoxes(); }
