
#define NUM_PARTICLES 20

typedef struct Particle {
    float x, y;
    float vx, vy;
    float mass;
} Particle;

typedef struct PressureCell {
    float pressure;
} PressureCell;

// Allocate memory for grids and particles

void updateParticlePositions(Particle* particles, float* velocities) {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x += velocities[i * 2];  // update x based on vx
        particles[i].y += velocities[i * 2 + 1]; // update y based on vy
    }
}

int simulate_fluid(void){ // main for this simulation

    return 0;
}
