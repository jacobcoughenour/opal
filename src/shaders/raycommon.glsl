struct hitPayload {
	vec3 hitValue;
	uint seed;
};

struct shadowPayload {
	bool isHit;
	uint seed;
};

struct Volume {
	vec3 position;
	vec3 size;
	int densityTextureId;
};

struct Sphere {
	vec3 center;
	float radius;
};

struct Aabb {
	vec3 minimum;
	vec3 maximum;
};

float min_elem(vec3 v) {
	return min(v.x, min(v.y, v.z));
}

float max_elem(vec3 v) {
	return max(v.x, max(v.y, v.z));
}

#define KIND_SPHERE 0
#define KIND_CUBE 1