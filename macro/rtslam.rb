require 'jafar/kernel'
require 'jafar/geom'
require 'jafar/rtslam/rtslam'
Jafar.register_module Jafar::Rtslam

module Jafar
module Rtslam

# The common origin is the initial position of the truth with null orientation
# in the truth frame.
# The truth file has to be manually cut and start simultaneously with slam file
#
# You can apply it on a sequence of log files :
# (0..99).each { |i| Rtslam::sync_truth("path/rtslam_#{sprintf('%03d',i)}.log", "path/mocap_trunc.dat", "path/slamocap_#{sprintf('%03d',i)}.log") }
#
# TODO:
# - deal correctly with angles and modulo for interpolation
# - read full cov matrix for slam, for real 6D NEES
	
def Rtslam.sync_truth(slam_filename, truth_filename, res_filename)
	
	# slam
	# slam inertial
	truth2rtruth_arr = [-0.013, -0.005, -0.012, 0, 0, 0]
	slam2rslam_arr = [0, 0, 0, 0, 0, Math::PI]
	slam_scale=1
	slam_ncol=45
	
	slam_t_col=0
	slam_x_col=1
	slam_y_col=2
	slam_z_col=3
	slam_w_col=8
	slam_p_col=9
	slam_r_col=10

	slam_sx_col=23
	slam_sy_col=24
	slam_sz_col=25
	slam_sw_col=30
	slam_sp_col=31
	slam_sr_col=32
	
	# slam constvel
# 	truth2rtruth_arr = [-0.015, 0.00, 0.02, 0, 0, 0]
# 	slam2rslam_arr = [0, 0, 0, 0, 0, 0]
# 	slam_scale=2.05
# 	slam_ncol=33
# 	
# 	slam_t_col=0
# 	slam_x_col=1
# 	slam_y_col=2
# 	slam_z_col=3
# 	slam_w_col=8
# 	slam_p_col=9
# 	slam_r_col=10
# 
# 	slam_sx_col=17
# 	slam_sy_col=18
# 	slam_sz_col=19
# 	slam_sw_col=24
# 	slam_sp_col=25
# 	slam_sr_col=26


	# truth
	truth_ncol=20

	truth_t_col=0
	truth_x_col=1
	truth_y_col=2
	truth_z_col=3
	truth_w_col=19
	truth_p_col=18
	truth_r_col=17
	
	truth_sx_col=-1
	truth_sy_col=-1
	truth_sz_col=-1
	truth_sw_col=-1
	truth_sp_col=-1
	truth_sr_col=-1

	truth_sx=0.001
	truth_sy=0.001
	truth_sz=0.001
	truth_sw=2.0/200.0
	truth_sp=2.0/200.0
	truth_sr=2.0/200.0


	# transforms
	truth2rtruth_vec = Jmath::arrayToVec(truth2rtruth_arr)
	truth2rtruth_t3d = Geom::T3DEuler.new(truth2rtruth_vec)

	slam2rslam_vec = Jmath::arrayToVec(slam2rslam_arr)
	slam2rslam_t3d = Geom::T3DEuler.new(slam2rslam_vec)

	
	slam_file = File.open(slam_filename)
	truth_file = File.open(truth_filename)
	res_file = File.open(res_filename,"w")

	res_line = "#t \
slam_x slam_y slam_z slam_w slam_p slam_r \
slam_sx slam_sy slam_sz slam_sw slam_sp slam_sr \
truth_x truth_y truth_z truth_w truth_p truth_r \
truth_sx truth_sy truth_sz truth_sw truth_sp truth_sr \
error_x error_y error_z error_w error_p error_r \
error_sx error_sy error_sz error_sw error_sp error_sr \
nees_x nees_y nees_z nees_w nees_p nees_r \
nees"
	res_file.puts(res_line)
	
	init = true
	
	truth_final_t = 0
	truth_prev_t = 0
	rtruth_prev_t3d = Geom::T3DEuler.new(true)
	rtruth_raw_t3d = Geom::T3DEuler.new(true)
	rtruth_final_t3d = Geom::T3DEuler.new(true)
	rslam_raw_t3d = Geom::T3DEuler.new(true)
	truth_transfo_t3d = Geom::T3DEuler.new(false)
	slam_transfo_t3d = Geom::T3DEuler.new(false)
	error_final_t3d = Geom::T3DEuler.new(true)

	truthP_raw_arr = [truth_sx, truth_sy, truth_sz, truth_sw, truth_sp, truth_sr]
	truthP_raw_vec = Jmath::arrayToVec(truthP_raw_arr)
	
	# get slam line
	while (slam_line = slam_file.gets)
		if slam_line[0] == 35 or slam_line[0] == 0 or slam_line[0] == 10 then next end
		slam_vec = slam_line.split(' ').map{|s| s.to_f}
		if slam_vec.length < slam_ncol then next end
		
		slam_raw_t = slam_vec[slam_t_col]
		slam_raw_arr = [slam_vec[slam_x_col]*slam_scale, slam_vec[slam_y_col]*slam_scale, slam_vec[slam_z_col]*slam_scale,
		                slam_vec[slam_w_col]           , slam_vec[slam_p_col]           , slam_vec[slam_r_col]           ]
		slamP_raw_arr = [slam_vec[slam_sx_col]*slam_scale, slam_vec[slam_sy_col]*slam_scale, slam_vec[slam_sz_col]*slam_scale,
		                 slam_vec[slam_sw_col]           , slam_vec[slam_sp_col]           , slam_vec[slam_sr_col]           ]
		slam_raw_vec = Jmath::arrayToVec(slam_raw_arr)
		slamP_raw_vec = Jmath::arrayToVec(slamP_raw_arr)
		slam_raw_t3d = Geom::T3DEuler.new(slam_raw_vec, slamP_raw_vec)
		
		if init then slam_transfo_t = slam_raw_t  end
		slam_final_t = slam_raw_t - slam_transfo_t
		
		# get truth line
		if init or truth_final_t < slam_final_t then
			if not init then truth_prev_t = truth_final_t; rtruth_prev_t3d = rtruth_final_t3d end
			
			while (truth_line = truth_file.gets)
				if truth_line[0] == 35 or truth_line[0] == 0 or truth_line[0] == 10 then next end
				truth_vec = truth_line.split(' ').map{|s| s.to_f}
				if truth_vec.length < truth_ncol then next end
				
				truth_raw_t = truth_vec[truth_t_col]
				truth_raw_arr = [truth_vec[truth_x_col], truth_vec[truth_y_col], truth_vec[truth_z_col],
												 truth_vec[truth_w_col], truth_vec[truth_p_col], truth_vec[truth_r_col]]
				truth_raw_vec = Jmath::arrayToVec(truth_raw_arr)
				truth_raw_t3d = Geom::T3DEuler.new(truth_raw_vec, truthP_raw_vec)
				
				if init then truth_transfo_t = truth_raw_t  end
				truth_final_t = truth_raw_t - truth_transfo_t
					
				if init or truth_final_t >= slam_final_t then break end
			end
		end
		
		# convert to robot center
		rtruth_raw_t3d = Geom::T3D::composeToEuler(truth_raw_t3d, truth2rtruth_t3d)
		rslam_raw_t3d = Geom::T3D::composeToEuler(slam_raw_t3d, slam2rslam_t3d)
		
		# 
		if (init) then
			rslam_final_arr = [0, 0, 0, rtruth_raw_t3d.getX.get(3), rtruth_raw_t3d.getX.get(4), rtruth_raw_t3d.getX.get(5)]
			rslam_final_vec = Jmath::arrayToVec(rslam_final_arr)
			rslam_final_t3d = Geom::T3DEuler.new(rslam_final_vec, true)
			rtruth_final_t3d = Geom::T3DEuler.new(rslam_final_vec, truthP_raw_vec)
			rtruth_inter_t3d = rtruth_final_t3d
			
			truth_transfo_t3d = Geom::T3D::composeToEuler(rtruth_final_t3d, Geom::T3D::invToEuler(rtruth_raw_t3d))
			slam_transfo_t3d = Geom::T3D::composeToEuler(rslam_final_t3d, Geom::T3D::invToEuler(rslam_raw_t3d))
# print(Jafar::Geom::print(truth_raw_t3d))
# print(Jafar::Geom::print(truth2rtruth_t3d))
# print(Jafar::Geom::print(rtruth_raw_t3d))
# print(Jafar::Geom::print(slam_raw_t3d))
# print(Jafar::Geom::print(rslam_raw_t3d))
# print(Jafar::Geom::print(truth_transfo_t3d))
# print(Jafar::Geom::print(slam_transfo_t3d))
			
			# remove cov
			truth_transfo_t3d.set(truth_transfo_t3d.getX)
			slam_transfo_t3d.set(slam_transfo_t3d.getX)
			
			init = false
		else
			rslam_final_t3d = Geom::T3D::composeToEuler(slam_transfo_t3d, rslam_raw_t3d)
			rtruth_final_t3d = Geom::T3D::composeToEuler(truth_transfo_t3d, rtruth_raw_t3d)
			
			# interpolation FIXME deal better with angles and modulo
			coeff = (slam_final_t - truth_prev_t) / (truth_final_t - truth_prev_t)
			rtruth_inter_t3d = Geom::T3DEuler.new(
				rtruth_prev_t3d.getX    + (rtruth_final_t3d.getX    - rtruth_prev_t3d.getX   ) * coeff,
				rtruth_prev_t3d.getXCov + (rtruth_final_t3d.getXCov - rtruth_prev_t3d.getXCov) * coeff)
# 			for i in (3..5)
# 				diff = rtruth_final_t3d.getX(i) - rtruth_prev_t3d.getX(i)
# 				if diff > Math::PI
# 				rtruth_prev_t3d.getX * (1-coeff) + rtruth_final_t3d.getX * coeff
# 				
# 				if rtruth_prev_t3d.getX(i) < -pi/2 and rtruth_final_t3d.getX(i) > pi/2 then
					
		end
		
		# error
		error_final_t3d = Geom::T3D::composeToEuler(rslam_final_t3d, Geom::T3D::invToEuler(rtruth_inter_t3d))
		#error_final_nees = Jmath::inner_prod(error_final_t3d.getX, Jmath::mul(Jmath::inv(error_final_t3d.getXCov), error_final_t3d.getX))
		error_final_nees = Jmath::prod_xt_P_x(error_final_t3d.getXCov, error_final_t3d.getX)

		rslam_final_t3d_dev = rslam_final_t3d.getXStdDev
		rtruth_final_t3d_dev = rtruth_inter_t3d.getXStdDev
		error_final_t3d_dev = error_final_t3d.getXStdDev
		
		res_line = "\
#{slam_final_t} \
#{rslam_final_t3d.getX.get(0)} #{rslam_final_t3d.getX.get(1)} #{rslam_final_t3d.getX.get(2)} \
#{rslam_final_t3d.getX.get(3)} #{rslam_final_t3d.getX.get(4)} #{rslam_final_t3d.getX.get(5)} \
#{rslam_final_t3d_dev.get(0)} #{rslam_final_t3d_dev.get(1)} #{rslam_final_t3d_dev.get(2)} \
#{rslam_final_t3d_dev.get(3)} #{rslam_final_t3d_dev.get(4)} #{rslam_final_t3d_dev.get(5)} \
#{rtruth_inter_t3d.getX.get(0)} #{rtruth_inter_t3d.getX.get(1)} #{rtruth_inter_t3d.getX.get(2)} \
#{rtruth_inter_t3d.getX.get(3)} #{rtruth_inter_t3d.getX.get(4)} #{rtruth_inter_t3d.getX.get(5)} \
#{rtruth_final_t3d_dev.get(0)} #{rtruth_final_t3d_dev.get(1)} #{rtruth_final_t3d_dev.get(2)} \
#{rtruth_final_t3d_dev.get(3)} #{rtruth_final_t3d_dev.get(4)} #{rtruth_final_t3d_dev.get(5)} \
#{error_final_t3d.getX.get(0)} #{error_final_t3d.getX.get(1)} #{error_final_t3d.getX.get(2)} \
#{error_final_t3d.getX.get(3)} #{error_final_t3d.getX.get(4)} #{error_final_t3d.getX.get(5)} \
#{error_final_t3d_dev.get(0)} #{error_final_t3d_dev.get(1)} #{error_final_t3d_dev.get(2)} \
#{error_final_t3d_dev.get(3)} #{error_final_t3d_dev.get(4)} #{error_final_t3d_dev.get(5)} \
#{Jmath::sqr(error_final_t3d.getX.get(0))/error_final_t3d.getXCov.get(0,0)} \
#{Jmath::sqr(error_final_t3d.getX.get(1))/error_final_t3d.getXCov.get(1,1)} \
#{Jmath::sqr(error_final_t3d.getX.get(2))/error_final_t3d.getXCov.get(2,2)} \
#{Jmath::sqr(error_final_t3d.getX.get(3))/error_final_t3d.getXCov.get(3,3)} \
#{Jmath::sqr(error_final_t3d.getX.get(4))/error_final_t3d.getXCov.get(4,4)} \
#{Jmath::sqr(error_final_t3d.getX.get(5))/error_final_t3d.getXCov.get(5,5)} \
#{error_final_nees}"
		
		res_file.puts(res_line)
	end


	slam_file.close
	truth_file.close
	res_file.close

end

end
end
			
