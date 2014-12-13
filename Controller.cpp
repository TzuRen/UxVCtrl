// Prevent Visual Studio Intellisense from defining _WIN32 and _MSC_VER when we use 
// Visual Studio to edit Linux or Borland C++ code.
#ifdef __linux__
#	undef _WIN32
#endif // __linux__
#if defined(__GNUC__) || defined(__BORLANDC__)
#	undef _MSC_VER
#endif // defined(__GNUC__) || defined(__BORLANDC__)

#include "Controller.h"

THREAD_PROC_RETURN_VALUE ControllerThread(void* pParam)
{
	CHRONO chrono;
	double dt = 0, t = 0, t0 = 0;
	double delta_d = 0; // For distance control.
	double delta_angle = 0; // For heading control.
	double wtheta_prev = 0; // For heading control.
	double integral = 0; // For heading control.
	double delta_z = 0; // For depth control.
	double delta_asf = 0; // For altitude w.r.t. sea floor control.

	UNREFERENCED_PARAMETER(pParam);

	StartChrono(&chrono);

	for (;;)
	{
		// Optimization : should compute the x=Center(xhat),... only once at each loop?

		mSleep(50);
		t0 = t;
		GetTimeElapsedChrono(&chrono, &t);
		dt = t-t0;

		//printf("ControllerThread period : %f s.\n", dt);

		EnterCriticalSection(&StateVariablesCS);

		// The order here gives some kind of priority...

		if (bLineFollowingControl)
		{
			double norm_ba = 0, norm_ma = 0, norm_bm = 0, sinalpha = 0, phi = 0, e = 0;

			// Should only be computed when the line changes...
			norm_ba = sqrt(pow(wxb-wxa,2)+pow(wyb-wya,2)); // Length of the line (norm of b-a).
			phi = atan2(wyb-wya,wxb-wxa); // Angle of the line.

			norm_ma = sqrt(pow(Center(xhat)-wxa,2)+pow(Center(yhat)-wya,2)); // Distance from the beginning of the line (norm of m-a).	
			norm_bm = sqrt(pow(wxb-Center(xhat),2)+pow(wyb-Center(yhat),2)); // Distance to the destination waypoint of the line (norm of b-m).	

			if ((norm_ma != 0)&&(norm_ba != 0))
			{
				sinalpha = ((wxb-wxa)*(Center(yhat)-wya)-(wyb-wya)*(Center(xhat)-wxa))/(norm_ma*norm_ba);
			}
			else
			{
				sinalpha = 0;
			}

			e = norm_ma*sinalpha; // Distance to the line (signed).

			wtheta = LineFollowing(phi, e, gamma_infinite, radius);
		}

		if (bWaypointControl)
		{
			wtheta = atan2(wy-Center(yhat), wx-Center(xhat));
		}

		// Low-level controls.

		if (bDistanceControl)
		{
			delta_d = dist-wd;
			if (delta_d > wdradius) u = fabs(wu);
			else if (delta_d < -wdradius) u = -fabs(wu); 
			else u = 0;
		}

		if (bBrakeControl)
		{
			if (Center(vxyhat) > 0.05) u = -u_max;
			else if (Center(vxyhat) < -0.05) u = u_max;
			else u = 0;
		}

		if (bHeadingControl)
		{
			if (wtheta != wtheta_prev)
			{
				integral = 0;
			}

			delta_angle = Center(thetahat)-wtheta;

			if (cos(delta_angle) > cosdelta_angle_threshold)
			{
				// PID-like control w.r.t. desired heading.
				//double error = 2.0*asin(sin(delta_angle))/M_PI;
				//double error = 2.0*atan(tan(delta_angle/2.0))/M_PI; // Singularity at tan(M_PI/2)...
				double error = fmod_2PI(delta_angle)/M_PI;
				double abserror = fabs(error);
				double speed = Center(omegahat)/omegamax;

				if ((robid & SAUCISSE_LIKE_ROBID_MASK)||(robid == SUBMARINE_SIMULATOR_ROBID))
				{
					if (error > 0) uw = -Kp*sqrt(abserror)-Kd1*speed/(Kd2+abserror)-Ki*integral;
					else uw = Kp*sqrt(abserror)-Kd1*speed/(Kd2+abserror)-Ki*integral;
				}
				else if (robid & CISCREA_ROBID_MASK) 
				{
					//uw = -Kp*error
					//	-(Kd1+Kd2*error*error*abserror)*Center(omegahat)*(fabs(Center(omegahat))>uw_derivative_max)
					//	-Ki*integral;
					uw = -Kp*error-Kd1*speed-Ki*integral;
				}
				else
				{
					//// We still (probably...) have to avoid the singularity at tan(M_PI/2)...
					//uw = -Kp*atan(tan(delta_angle/2.0))-Kd1*Center(omegahat)-Ki*integral;
					uw = -Kp*error-Kd1*speed-Ki*integral;
				}

				integral = integral+error*dt;

				// Limit the integral.
				if (Ki*integral > uw_integral_max) integral = uw_integral_max/Ki;
				if (Ki*integral < -uw_integral_max) integral = -uw_integral_max/Ki;
			}
			else
			{
				// Bang-bang control if far from desired heading.
				if (sin(delta_angle) > 0) uw = -uw_max; else uw = uw_max;
				integral = 0;
			}

			wtheta_prev = wtheta;
		}
		else
		{
			integral = 0;
		}

		if (bAltitudeSeaFloorControl)
		{
			delta_asf = altitude_sea_floor-wasf;
			if (delta_asf > wzradiushigh) uv = -uv_max;
			else if (delta_asf < -wzradiuslow) uv = uv_max; 
			else uv = 0;
		}

		if (bDepthControl)
		{
			delta_z = Center(zhat)-wz;
			if (delta_z > wzradiushigh) uv = -uv_max;
			else if (delta_z < -wzradiuslow) uv = uv_max; 
			else uv = 0;
		}

		u = (u > u_max)? u_max: u;
		u = (u < -u_max)? -u_max: u;
		uw = (uw > uw_max)? uw_max: uw;
		uw = (uw < -uw_max)? -uw_max: uw;
		uv = (uv > uv_max)? uv_max: uv;
		uv = (uv < -uv_max)? -uv_max: uv;

		//u1 = (u+uw)/2;
		//u2 = (u-uw)/2;
		// Force to slow down to be able to rotate correctly when too fast...
		if (u_coef*u+uw_coef*abs(uw) > 1)
		{
			double uw_boost = u_coef*u+uw_coef*abs(uw)-1;
			u1 = u_coef*u+uw_coef*uw-uw_boost;
			u2 = u_coef*u-uw_coef*uw-uw_boost;
		}
		else if (u_coef*u-uw_coef*abs(uw) < -1)
		{
			double uw_boost = -(u_coef*u-uw_coef*abs(uw))-1;
			u1 = u_coef*u+uw_coef*uw+uw_boost;
			u2 = u_coef*u-uw_coef*uw+uw_boost;
		}
		else
		{
			u1 = u_coef*u+uw_coef*uw;
			u2 = u_coef*u-uw_coef*uw;
		}
		u3 = uv;

		u1 = (u1<1)?u1:1;
		u1 = (u1>-1)?u1:-1;
		u2 = (u2<1)?u2:1;
		u2 = (u2>-1)?u2:-1;
		u3 = (u3<1)?u3:1;
		u3 = (u3>-1)?u3:-1;

		LeaveCriticalSection(&StateVariablesCS);

		if (bExit) break;
	}

	StopChrono(&chrono, &t);

	return 0;
}