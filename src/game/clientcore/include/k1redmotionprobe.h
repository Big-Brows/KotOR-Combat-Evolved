// Pose diagnostics for red resources; no scene or player state is changed.
Vector redProbeGuardGap;
bool RedOriginalGuardProbe(const char* resource,Gob* body) {
    const char* path=!strcmp(resource,"k1jadred")?"override/red-guard-npc.txt":"override/red-guard-player.txt";
    FILE* file=fopen(path,"r");if(!file)return false;
    char clip[32],bone[64];float time,px,py,pz,qx,qy,qz,qw,lastTime=-1;
    std::string previous;bool ok=true;int joints=0,poses=0;float positionError=0,rotationError=0;
    while(fscanf(file,"%31s %f %63s %f %f %f %f %f %f %f",clip,&time,bone,&px,&py,&pz,&qx,&qy,&qz,&qw)==10) {
        if(previous!=clip || time!=lastTime) {
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            if(!body->PlayAnimation(clip,0,AnimRun::kAnimNoBlendIn,time)){ok=false;break;}
            body->m_nLastAnimateTick=-1;body->Animate(.000001f);previous=clip;lastTime=time;++poses;
        }
        Part* part=body->GetPart(bone);if(!part){ok=false;break;}
        float pe=magnitude(part->m_vLocalPosition-Vector(px,py,pz));
        float qe=1-std::fabs(dot(normalize(part->m_qLocalOrientation),normalize(Quaternion(qw,qx,qy,qz))));
        if((pe>=.00002f || qe>=.00001f) && joints<60)
            fprintf(stderr,"K1_RED_GUARD_MISMATCH clip=%s bone=%s t=%g position_error=%g rotation_error=%g actual_p=%g,%g,%g actual_q=%g,%g,%g,%g\n",clip,bone,time,pe,qe,part->m_vLocalPosition.x,part->m_vLocalPosition.y,part->m_vLocalPosition.z,part->m_qLocalOrientation.w,part->m_qLocalOrientation.x,part->m_qLocalOrientation.y,part->m_qLocalOrientation.z);
        positionError=std::max(positionError,pe);rotationError=std::max(rotationError,qe);
        ok=std::isfinite(pe+qe) && pe<.00002f && qe<.00001f && ok;++joints;
    }
    fclose(file);
    fprintf(stderr,"K1_RED_ORIGINAL_GUARD_%s model=%s poses=%d joints=%d position_error=%g rotation_error=%g\n",ok?"PASS":"FAIL",resource,poses,joints,positionError,rotationError);
    return ok && poses==6 && joints>100;
}
bool RedMotionComponentProbe(const char* resource) {
    CSWCAnimBase owner;
    if(!owner.LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY))return false;
    Gob* body=owner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    body->m_bAlwaysRendering=true;body->EnableForceAnimations();
    if(!RedOriginalGuardProbe(resource,body))return false;
    FILE* f=fopen("red-motion.csv","a");
    bool ok=true;
    for(int d=-1;d<7;++d) {
        const char* clip=d<0?"j_guard":fullAttacks[d];
        body->PlayAnimation(nullptr,1,0,0);body->Reset();
        if(!body->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0)){ok=false;break;}
        float at=0,length=0;body->GetAnimation(const_cast<char*>(clip),&at,&length);
        GripPose last;Vector previousVelocity;float peakStep=0,peakAcceleration=0,peakRotation=0;
        for(int frame=0;frame<=int(length*120);++frame) {
            body->Reset();body->SetAnimationTime(const_cast<char*>(clip),float(frame)/120);
            body->m_nLastAnimateTick=-1;body->Animate(.000001f);
            const auto p=ReadGrip(body);
            const Vector gap=Rotate(Inverse(p.hiltQ),p.left-p.hilt);
            if(d<0 && !frame && !strcmp(resource,"k1jared"))redProbeGuardGap=gap;
            const Vector velocity=(p.hilt-last.hilt)*120;
            const float rotation=frame?2*std::acos(std::min(1.f,std::fabs(dot(normalize(p.hiltQ),normalize(last.hiltQ))))):0;
            const float acceleration=frame>1?magnitude(velocity-previousVelocity):0;
            if(frame)peakStep=std::max(peakStep,magnitude(p.hilt-last.hilt));
            peakAcceleration=std::max(peakAcceleration,acceleration);peakRotation=std::max(peakRotation,rotation);
            if(f)fprintf(f,"%s,%s,%d,%g,%g,%g,%g,%g,%g,%g,%g\n",resource,clip,frame,p.hilt.x,p.hilt.y,p.hilt.z,rotation,acceleration,gap.x,gap.y,gap.z);
            ok=ok && std::isfinite(rotation+acceleration+gap.x+gap.y+gap.z);
            last=p;previousVelocity=velocity;
        }
        fprintf(stderr,"K1_RED_MOTION model=%s clip=%s step=%g acceleration=%g rotation=%g\n",resource,clip,peakStep,peakAcceleration,peakRotation);
        ok=peakRotation<.7f && ok;
    }
    if(f)fclose(f);
    int pairs=0,direct=0;float maximumRotation=0;
    for(int from=0;from<7;++from)for(int to=0;to<7;++to) {
        const auto& clip=redChains[from][to];
        if(!strcmp(clip.transition,"direct"))++direct;
        for(const char* name:{clip.full,clip.upper}) {
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            if(!body->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0))return false;
            float time=0,length=0;
            if(!body->GetAnimation(const_cast<char*>(name),&time,&length) || std::fabs(length-clip.length)>.00001f)return false;
            Quaternion previous;
            for(int frame=0;frame<=int(length*120);++frame) {
                body->Reset();body->SetAnimationTime(const_cast<char*>(name),float(frame)/120);body->m_nLastAnimateTick=-1;body->Animate(.000001f);
                const auto grip=ReadGrip(body);
                if(frame)maximumRotation=std::max(maximumRotation,2*std::acos(std::min(1.f,std::fabs(dot(normalize(previous),normalize(grip.hiltQ))))));
                previous=grip.hiltQ;
            }
        }
        float lo=0,hi=1;
        if(clip.start>0)ok=!K1SaberContact::ClipTime(0,clip.start-.0001f,clip.start,clip.end,lo,hi) && ok;
        ok=K1SaberContact::ClipTime(clip.start,(clip.start+clip.end)*.5f,clip.start,clip.end,lo,hi) && ok;
        ok=!K1SaberContact::ClipTime(clip.end+.001f,clip.length,clip.start,clip.end,lo,hi) && ok;
        ok=clip.end>clip.start && clip.length>clip.end && ok;++pairs;
    }
    fprintf(stderr,"K1_RED_JKA_PAIRS model=%s pairs=%d direct=%d max_rotation=%g damage_excluded_in_transition=1\n",resource,pairs,direct,maximumRotation);
    ok=pairs==49 && direct==6 && maximumRotation<1.f && ok;
    return ok;
}
