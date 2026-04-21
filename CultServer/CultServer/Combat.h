#pragma once

#include "Protocol.h"

bool line_sphere_intersect(const FVector&, const FVector&, const FVector&, double);

void baton_sweep(int, HitPacket*);

void line_trace(int, HitPacket*);