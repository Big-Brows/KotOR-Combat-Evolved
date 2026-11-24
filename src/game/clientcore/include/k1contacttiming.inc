// Diagnostic hooks only; the native damage queue/effect ownership is unchanged.
void K1EngageContactEffectQueued(unsigned long long attacker,unsigned long long target,CGameEffect* effect) {
    if(!ContactTimingEnabled() || !effect || !damageContact || damageContact->attacker!=attacker || damageContact->target!=target)return;
    const auto world=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime(),wall=ContactWallUs();
    if(contactEffectClocks.size()>4096){contactEffectClocks.clear();fprintf(stderr,"K1_CONTACT_CLOCK_RESET reason=capacity\n");}
    contactEffectClocks[effect->m_effectId]={world,wall,damageContact->at,damageContact->wallAt?damageContact->wallAt:wall,attacker,target,damageContact->serial,damageContact->blade};
    fprintf(stderr,"K1_CONTACT_NATIVE_QUEUE effect=%llu actor=%llu target=%llu serial=%d hand=%d contact_wall_us=%lld contact_world_us=%lld mode=%d\n",
        effect->m_effectId,attacker,target,damageContact->serial,damageContact->blade,wall-(damageContact->wallAt?damageContact->wallAt:wall),world-damageContact->at,int(state.mode));
}
void K1EngageContactDamageApplied(CSWSCreature* target,CGameEffect* effect,int damage) {
    if(!ContactTimingEnabled() || !effect || !target)return;
    auto found=contactEffectClocks.find(effect->m_effectId);
    if(found==contactEffectClocks.end() || found->second.target!=target->m_objectId)return;
    const auto clock=found->second;contactEffectClocks.erase(found);
    const auto world=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime(),wall=ContactWallUs();
    fprintf(stderr,"K1_CONTACT_HEALTH effect=%llu actor=%llu target=%llu serial=%d hand=%d native_wall_us=%lld native_world_us=%lld total_wall_us=%lld total_world_us=%lld damage=%d hp=%d mode=%d\n",
        effect->m_effectId,clock.attacker,clock.target,clock.serial,clock.hand,wall-clock.wall,world-clock.world,wall-clock.contactWall,world-clock.contactWorld,damage,target->GetCurrentHitPoints(0),int(state.mode));
}
