/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

	//random seed
	static default_random_engine random_gen;

  //Define Gaussian noise generators on GPS sensor
  normal_distribution<double> x_Norm(0, std[0]);
  normal_distribution<double> y_Norm(0, std[1]);
  normal_distribution<double> theta_Norm(0, std[2]);

	//Set to some random int
	num_particles = 200;
	for(int i = 0; i< num_particles; i++)
	{
		//initialize particles with noisy sensor data
		Particle particle;
		particle.id = i;
		particle.x = x + x_Norm(random_gen);
		particle.y = y + y_Norm(random_gen);
		particle.theta = theta + theta_Norm(random_gen);
		//Set all default values of initialized particles to weight 1
		particle.weight = 1.;
		//add to particle list/vector of particle filter
		particles.push_back(particle);
	}

	//initialized particle filter
	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	//random seed
	static default_random_engine random_gen;

	//Define Gaussian noise generators on GPS sensor
  normal_distribution<double> x_Norm(0, std_pos[0]);
  normal_distribution<double> y_Norm(0, std_pos[1]);
  normal_distribution<double> theta_Norm(0, std_pos[2]);

	for (int i=0; i<num_particles; i++)
	{
		//if yaw rate measurements taken within 1e5 seconds assume yaw_rate = 0 and therefore
		// use standard trig
		if(abs(yaw_rate)<1e-5)
		{
			particles[i].x+=velocity*delta_t*cos(particles[i].theta);
		}
		else
		{
			//use standard prediction equations refered to in Udacity prediction
			//in particle filter class
			particles[i].x += velocity / yaw_rate * (sin(particles[i].theta + yaw_rate*delta_t) - sin(particles[i].theta));
      particles[i].y += velocity / yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + yaw_rate*delta_t));
      particles[i].theta += yaw_rate * delta_t;
		}

		//Add measurement noise
		particles[i].x += x_Norm(random_gen);
    particles[i].y += y_Norm(random_gen);
    particles[i].theta += theta_Norm(random_gen);
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to
	//   implement this method and use it as a helper during the updateWeights phase.

	for (int i = 0; i < observations.size(); i++)
	{
    LandmarkObs observation = observations[i];
		//magic numbers for search O(predicted.size()*observed.size())
		//initialization
    double minDist = 1e19;
    int minParticleId = -1;

    for (int j = 0; j < predicted.size(); j++)
		{
      LandmarkObs prediction = predicted[j];
			//Helper function from Helper.h
      double Currentdistance = dist(observation.x, observation.y, prediction.x, prediction.y);

      // find the predicted landmark nearest the current observed landmark
      if (Currentdistance < minDist) {
        minDist = Currentdistance;
        minParticleId= prediction.id;
      }
    }
    observations[i].id = minParticleId;
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		std::vector<LandmarkObs> observations, Map map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html
	for (int i = 0; i < num_particles; i++)
	{
		// get the particle details
		double p_x = particles[i].x;
		double p_y = particles[i].y;
		double p_theta = particles[i].theta;

		//Holds all landmarks within particle range
		vector<LandmarkObs> predictions;

		for (int j = 0; j < map_landmarks.landmark_list.size(); j++)
		{
			float l_x = map_landmarks.landmark_list[j].x_f;
			float l_y = map_landmarks.landmark_list[j].y_f;
			int l_id = map_landmarks.landmark_list[j].id_i;

			if (abs(dist(p_x, p_y, l_x, l_y)) <= sensor_range)
			{
				predictions.push_back(LandmarkObs{ l_id, l_x, l_y });
			}
		}
		// Transform sensor observations coordinates to map and particle coordinates
		vector<LandmarkObs> transformed_observations;

		for (int j = 0; j < observations.size(); j++)
		{
			double t_x = cos(p_theta)*observations[j].x - sin(p_theta)*observations[j].y + p_x;
			double t_y = sin(p_theta)*observations[j].x + cos(p_theta)*observations[j].y + p_y;
			transformed_observations.push_back(LandmarkObs{ observations[j].id, t_x, t_y });
		}

		//Find closest predicted particles to observations
		dataAssociation(predictions, transformed_observations);

		// set weights to 1.0
		particles[i].weight = 1.0;

		for (int j = 0; j < transformed_observations.size(); j++)
		{
			// placeholders for observation and associated prediction coordinates
			double o_x = transformed_observations[j].x;
			double o_y = transformed_observations[j].y;
			int o_id = transformed_observations[j].id;

			// get the x,y coordinates of the prediction associated with the current observation
			double pr_x, pr_y;
			for (int k = 0; k < predictions.size(); k++)
			{
				if (predictions[k].id == o_id) {
					pr_x = predictions[k].x;
					pr_y = predictions[k].y;
				}
			}
			//Weights calculations for observations using mult-variate Gaussian
			double std_x = std_landmark[0];
			double std_y = std_landmark[1];
			double observation_w = (1/(2*M_PI*std_x*std_y)) * exp( -( pow(pr_x-o_x,2)/(2*pow(std_x, 2)) + (pow(pr_y-o_y,2)/(2*pow(std_y, 2))) ) );
			// product of this obersvation weight with total observations weight
			particles[i].weight *= observation_w;
		}
  }
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight.
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

  //instantiate random seed
	static default_random_engine random_gen;

	//new particles buffer
	vector<Particle> new_particles;

   // Extract all current weights
   vector<double> weights;
   for (int i = 0; i < num_particles; i++) {
     weights.push_back(particles[i].weight);
   }

   // generate random starting index for resampling wheel
   uniform_int_distribution<int> random_int(0, num_particles-1);
   auto index = random_int(random_gen);

   //get most accurate particle weight
   double max_weight = *max_element(weights.begin(), weights.end());

   // uniform random distribution [0.0, max_weight]
   uniform_real_distribution<double> random_particle_dist(0.0, max_weight);

   double beta = 0.0;

   //resample wheel taken from Udacity classes
   for (int i = 0; i < num_particles; i++) {
     beta +=random_particle_dist(random_gen) * 2.0;
     while (beta > weights[index]) {
       beta -= weights[index];
       index = (index + 1) % num_particles;
     }
     new_particles.push_back(particles[index]);
   }
   particles = new_particles;
}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	//Clear the previous associations
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations= associations;
 	particle.sense_x = sense_x;
 	particle.sense_y = sense_y;

 	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
