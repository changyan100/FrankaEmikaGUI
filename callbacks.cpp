#include "callbacks.h"



void MainWindow::callbacks(void)
{
    // Execute code here
//    qInfo() << connectedGamepad.joystickValues[0];
//    qDebug() << "Code callback executing...";

    //We connect Franka every turns of callback , could be improved...


    if (isRobotreading){
        franka::Robot robot(fci_ip);
        //read franka robot pose
        franka::RobotState initial_state = robot.readOnce();
        // EE in base frame, 4x4 matrix: initial_state.O_T_EE.data();
        Eigen::Affine3d initial_transform(Eigen::Matrix4d::Map(initial_state.O_T_EE.data()));
        Eigen::Vector3d position_d(initial_transform.translation());
    //    position_d = position_d*1000;//from m to mm
    //    Eigen::Quaterniond orientation_d(initial_transform.linear());
        Eigen::Matrix3d rotation(initial_transform.linear());
        Eigen::Vector3d eulerangle = rotation.eulerAngles(0, 1, 2);

        Eigen::Affine3d EEinFlange(Eigen::Matrix4d::Map(initial_state.F_T_EE.data()));
        Eigen::Vector3d EEinFpos(EEinFlange.translation());
    //    EEinFpos = EEinFpos*1000; //from m to mm
    //    Eigen::Quaterniond orientation_d(initial_transform.linear());
        Eigen::Matrix3d EEinFrot(EEinFlange.linear());
        Eigen::Vector3d EEinFeulerangle = EEinFrot.eulerAngles(0, 1, 2);


        Eigen::Map<Eigen::Matrix<double, 7, 1>> initial_tau_measured(initial_state.tau_J.data());


        //default unit is meter and rad, so we keep that
        Robot_tip_posisition[0] = position_d[0];
        Robot_tip_posisition[1] = position_d[1];
        Robot_tip_posisition[2] = position_d[2];

        Robot_orient[0] = eulerangle[0];
        Robot_orient[1] = eulerangle[1];
        Robot_orient[2] = eulerangle[2];

        for(int i=0; i<7; i++){
            Robot_joint[i] = initial_state.q.data()[i];
        }

    }


    // ~~~~~~~~~~~~~~~~~~~~~~~~ GENERAL CALLBACKS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
    // These are commands that are executed every iteration no matter the settings
    // to ensure that the system is running properly. As such, they should not be
    // modified unless you know what you are doing.
    // moved from Adam's code

    // Read Thermocouples and Current Monitors
    if (s826.boardConnected)
    {
        // Read all 16 input channels and store in pass-by-reference array:
        int err = s826.analogReadAll(inputAnalogVoltages);
//        qInfo() << err;
    }

    for (int t = 0; t < 8; t++)
    {
        // Record new temperatures and currents from conversions
        // Input Analog Voltages are as follows:
        // AIN0 - AIN7  CURRENT SENSE FROM EM0-EM7, respectively
        // AIN8 - AIN15 THERMOCOUPLE SENSE FROM EM0 - EM7, respectively
        measuredCurrents[t] = inputAnalogVoltages[t]*currentSenseAdj[t]; // [A] read from amplifiers
        measuredTemperatures[t] = inputAnalogVoltages[t+8]*temperatureSenseAdj[t]; // [deg C] read from thermocouples
        // Check that the temperature in any core is not above the max value
        if (measuredTemperatures[t] > maxAllowableTemp)
        {
            overheatingFlag = true;
            // set all currents to 0 and reset all desired field strengths
            qWarning() << "System is Overheating!!!" ;
            updateCurrents();
            qInfo() << "Currents Cleared.";
            // at the end of callbacks, re-evaluate the temperatures.
        }
    }

    // READ FROM DAQ
    if (DAQ.isEnabled())
    {
        // Read analog inputs from the DAQ by reading values and passing by ref.
        DAQ.dataAcquisition8(DAQ.analogInputVoltages);
        DAQ.dataAcquisition8(DAQ.analogRawInputVoltages); //record the raw data without any change
//        DAQ.dataAcquisition();
        //Get Forces and torques from values
        double tempVoltages[6];
        double originalDAQVol[6];
        for (int v = 0; v<6; v++)
        {
            tempVoltages[v] = DAQ.analogInputVoltages[v]-ATINanoVoltageOffsets[v];
            originalDAQVol[v] = DAQ.analogInputVoltages[v];
        }

//        double A[6][6] = {0.0};
//        double J[6] = {0.0};
////        MatrixMultVect6(A, tempVoltages, J);
//        MatrixMultVect6(ATINanoCalibrationMatrix, tempVoltages, ATINanoForceTorque);
//        //this is for ATI force/torque output
//        for (int v = 0; v<6; v++)
//        {
//            DAQ.analogInputVoltages[v] = ATINanoForceTorque[v];
//        }
        //this is for gaussmeter output
//        for (int v = 0; v<3; v++)
//        {
//            DAQ.analogInputVoltages[v] = originalDAQVol[v]*gaussmeterCalibratedConstants[v];
//        }
    }


//    std::cout<<"robot EE posisiton is: " << position_d <<std::endl;
//    std::cout<<"robot EE orientation is: " << eulerangle <<std::endl;

//    std::cout<<"robot EEinF posisiton is: " << EEinFpos <<std::endl;
//    std::cout<<"robot EEinF orientation is: " << EEinFeulerangle <<std::endl;

//    std::cout<<"robot measured tau: "<<initial_tau_measured<<std::endl;


//    if(LogEnabled){
//         MainWindow::Record();
//    }


    if(CalibrationDataCollet)
    {
        if (!robotinitialized) //not initialized
        {
            franka::Robot robot(fci_ip);
            setDefaultBehavior(robot);
            // First move the robot to a suitable joint configuration
//            std::array<double, 7> q_goal = {{0, -M_PI_4, 0, -3 * M_PI_4, 0, M_PI_2, M_PI_4}};
            std::array<double, 7> q_goal = {{-0.0696155, 0.30357, -0.00032821, -2.22616, -0.0, 2.51505, 0.715763}};
            MotionGenerator motion_generator(0.5, q_goal);
            robot.control(motion_generator);
            std::cout << "Finished moving to initial joint configuration in callback." << std::endl;
            robotinitialized = true;
        }
        else
        {
            if (currentcount < currentloop)
            {
                if(robotloopisdone)
                {
                    // those code is copied from https://stackoverflow.com/questions/13445688/how-to-generate-a-random-number-in-c
                    // and is used to generate a random number, which is placed here for local use
                    std::random_device dev;
                    std::mt19937 rng(dev());
                    std::uniform_int_distribution<std::mt19937::result_type> dist6(0,maxCurrent*2); // distribution in range [a, b]
                    //

                    for (int i = 0; i < 8; i++)
                    {
                      cmdCoilCurrent[i] =  dist6(rng)-maxCurrent; //generate random current in the range of [-maxCurrent, maxcurrent]
                    }
                    qInfo() << "Currents are: "<<cmdCoilCurrent[0]<<", "<<cmdCoilCurrent[1]<<", "<<cmdCoilCurrent[2]<<", "<<cmdCoilCurrent[3]<<", "<<cmdCoilCurrent[4]<<", "<<cmdCoilCurrent[5]<<", "<<cmdCoilCurrent[6]<<", "<<cmdCoilCurrent[7];
                    updateCurrents_CalibrationOnly(cmdCoilCurrent);

                    robotloopisdone = false;
                }
                if(!robotloopisdone)
                {
                    if (robotmovecount < robotmoveloop)
                    {
                        // those code is copied from https://stackoverflow.com/questions/13445688/how-to-generate-a-random-number-in-c
                        // and is used to generate a random number, which is placed here for local use
    //                    double       leftlowercorner[3] = {-50.0, -50.0, 50.0}; //table origin is at the table center (0,0,0)
    //                    double       righttopcorner[3] = {50.0, 50.0, 100.0};
                        std::random_device dev1;
                        std::mt19937 rng1(dev1());
                        std::random_device dev2;
                        std::mt19937 rng2(dev2());
                        std::random_device dev3;
                        std::mt19937 rng3(dev3());
                        std::uniform_int_distribution<std::mt19937::result_type> pos_x(0,righttopcorner[0]*2); // distribution in range [0, 100]
                        std::uniform_int_distribution<std::mt19937::result_type> pos_y(0,righttopcorner[1]*2); // distribution in range [0, 100]
                        std::uniform_int_distribution<std::mt19937::result_type> pos_z(0,leftlowercorner[2]); // distribution in range [0, 50]
                        double robotposcmd[3]; //unit: m
                        robotposcmd[0] = (pos_x(rng1)-righttopcorner[0])*0.001; //rand [-50, 50]
                        robotposcmd[1] = (pos_y(rng2)-righttopcorner[1])*0.001; //rand [-50, 50]
                        robotposcmd[2] = (pos_z(rng3)+leftlowercorner[2])*0.001; //rand [50, 100]
    //                    I_command =  dist6(rng)-maxCurrent;


                        qInfo() << "command position in table frame is: "<<robotposcmd[0]<<", "<<robotposcmd[1]<<", "<<robotposcmd[2];

                        //covert cmd position in table frame to robot frame
                        Eigen::Vector4d pos_cmd(robotposcmd[0], robotposcmd[1], robotposcmd[2], 1);
                        Eigen::Vector4d pos_robot = transT2R*pos_cmd;

                        double abs_robotpos[3] = {pos_robot(0), pos_robot(1), pos_robot(2)};

                        qInfo() << "command position in robot frame is: "<<abs_robotpos[0]<<", "<<abs_robotpos[1]<<", "<<abs_robotpos[2];

                        //move robot in absolute position
                        franka::Robot robot(fci_ip);
                        try{
                            setDefaultBehavior(robot);
                            // Set additional parameters always before the control loop, NEVER in the control loop!

//                            std::array<double, 16> initial_pose;
                            double time = 0.0;

                            /// ----------------robot control loop---------------------------------
                            robot.control([=, &time](const franka::RobotState& robot_state,
                                                     franka::Duration period) -> franka::CartesianVelocities
//                            robot.control([&time, &initial_pose, &abs_robotpos]( const franka::RobotState& robot_state,
//                                                                 franka::Duration period) -> franka::CartesianPose
                            {
                              time += period.toSec();
//                              if (time == 0.0) {
//                                initial_pose = robot_state.O_T_EE_c; //Last commanded end effector pose of motion generation in base frame.
//                                //robot_state.O_T_EE; Measured end effector pose in base frame.
//                                //robot_state.O_T_EE_d; Last desired end effector pose of motion generation in base frame.
//                              }
//                              std::array<double, 16> new_pose = initial_pose;
                              double tolerance = 0.005; //2mm
                              double error[3];
                              double direction[3];
                              std::array<double, 16> current_pose = robot_state.O_T_EE; //not sure whether should use _d

//                              qInfo() << "command position in robot frame is: "<<abs_robotpos[0]<<", "<<abs_robotpos[1]<<", "<<abs_robotpos[2];
//                              qInfo() << "current robot position is: "<<current_pose[12]<<", "<<current_pose[13]<<", "<<current_pose[14];
                              for(int k=0; k<3; k++)
                              {
                                  double temp_e = abs_robotpos[k]-current_pose[12+k];
                                  if (temp_e>0)
                                     direction[k] = 1.0;
                                  else
                                      direction[k] = -1.0;

                                  if (abs(temp_e)>=tolerance)
                                      error[k] = temp_e;
                                  else
                                  {
                                      error[k] = 0.0;
                                      direction[k] = 0.0;
                                  }
                              }
//                              std::cout<<"pos error is: "<<error[0]<<" "<<error[1]<<" "<<error[2]<<std::endl;
//                              double detp = 0.00001; //0.1mm

//                              new_pose[12] = current_pose[12]+direction[0]*detp;
//                              new_pose[13] = current_pose[13]+direction[1]*detp;
//                              new_pose[14] = current_pose[14]+direction[2]*detp;
                              double v_x = 0.002;
                              double v_y = 0.002;
                              double v_z = 0.002;

                              franka::CartesianVelocities output = {{direction[0]*v_x, direction[1]*v_y, direction[2]*v_z, 0.0, 0.0, 0.0}};
                              if (abs(error[0])<tolerance && abs(error[1])<tolerance && abs(error[2])<tolerance) {
                                std::cout << std::endl << "Finished motion, shutting down example" << std::endl;
                                output = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
                                return franka::MotionFinished(output);
                              }
                              return output;
                            });
                        }catch (const franka::Exception& e) {
                        std::cout << e.what() << std::endl;
                        std::cout << "Running error recovery..." << std::endl;
                        robot.automaticErrorRecovery();
                        }

                        //read franka robot pose
                        franka::RobotState initial_state = robot.readOnce();
                        // EE in base frame, 4x4 matrix: initial_state.O_T_EE.data();
                        Eigen::Affine3d initial_transform(Eigen::Matrix4d::Map(initial_state.O_T_EE.data()));
                        Eigen::Vector3d position_d(initial_transform.translation());
                    //    position_d = position_d*1000;//from m to mm
                    //    Eigen::Quaterniond orientation_d(initial_transform.linear());
                        Eigen::Matrix3d rotation(initial_transform.linear());
                        Eigen::Vector3d eulerangle = rotation.eulerAngles(0, 1, 2);

                        Eigen::Affine3d EEinFlange(Eigen::Matrix4d::Map(initial_state.F_T_EE.data()));
                        Eigen::Vector3d EEinFpos(EEinFlange.translation());
                    //    EEinFpos = EEinFpos*1000; //from m to mm
                    //    Eigen::Quaterniond orientation_d(initial_transform.linear());
                        Eigen::Matrix3d EEinFrot(EEinFlange.linear());
                        Eigen::Vector3d EEinFeulerangle = EEinFrot.eulerAngles(0, 1, 2);


                        Eigen::Map<Eigen::Matrix<double, 7, 1>> initial_tau_measured(initial_state.tau_J.data());


                        //default unit is meter and rad, so we keep that
                        Robot_tip_posisition[0] = position_d[0];
                        Robot_tip_posisition[1] = position_d[1];
                        Robot_tip_posisition[2] = position_d[2];

                        Robot_orient[0] = eulerangle[0];
                        Robot_orient[1] = eulerangle[1];
                        Robot_orient[2] = eulerangle[2];

                        for(int i=0; i<7; i++){
                            Robot_joint[i] = initial_state.q.data()[i];
                        }

                    // record
                        if(LogEnabled){
                             MainWindow::Record();
                        }
                        else
                        {
                            qInfo()<<"log is not enabled!";
                        }

                        robotmovecount++;
                    }
                    else
                    {
                        robotmovecount = 0;
                        robotloopisdone = true;
                        currentcount++;
                        qInfo()<<"single current loop is done!";
                    }
                }
            }
            else
            {
//                qInfo()<<"calibration data collection is done! set current to zeros";
                double I_zeros[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
                updateCurrents_CalibrationOnly(I_zeros);
//                // set robot to intial pos
//                franka::Robot robot(fci_ip);
//                setDefaultBehavior(robot);
//                // First move the robot to a suitable joint configuration
//                std::array<double, 7> q_goal = {{0, -M_PI_4, 0, -3 * M_PI_4, 0, M_PI_2, M_PI_4}};
//                MotionGenerator motion_generator(0.5, q_goal);
//                robot.control(motion_generator);
//                std::cout << "Finished moving to initial joint configuration." << std::endl;
            }

        }


    }






    if (connectedGamepad.enabled) // Update Direct Local B-field
        {
            //// 1. DIRECT LOCAL B-FIELD CONTROL:
            //
            // Here the joystick directly controls the desired Bx, By, and Bz field components
            // in the tool frame. This assumes that the desired fields are relative to the
            // tool which, when at rest, points in the z-direction. The code takes the
            // desired local frame magnetic field values and computes the necessary field in
            // the global frame to be sent to the actuators.
            //
            // The assigned joystick axes are as follows:
            // Left Y-axis:     Bx field
            // Right X-axis:    By field
            // L2 and R2:       Bz field

    //        qDebug() << "Working";

            // Set desired B local field
            std::cout<<"joystick values: " <<connectedGamepad.joystickValues <<std::endl; // Increase nonlinearly to add sensitivity and control
    }




}
