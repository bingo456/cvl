/**
  * \author Sergio Agostinho - sergio(dot)r(dot)agostinho(at)gmail(dot)com
  * \date created: 2017/05/05
  * \date last modified: 2017/08/19
  * \file vgm_grabber.h
  * \brief Provides the implementation of a grabber for the
  * <a href="https://www.iit.it/research/lines/visual-geometry-and-modelling/datasets/514-vgm-6d-pose-of-texture-less-objects-dataset">
  * VGM-Dataset: 6D Pose of Texture-less Objects (ICRA 2016)</a>
  */
#pragma once
#ifndef CVL_IO_VGM_GRABBER_H_
#define CVL_IO_VGM_GRABBER_H_

#include <cvl/io/grabber.h>
#include <cvl/common/camera.h>
#include <cvl/common/eigen.h>
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>
#include <fstream>
#include <string>

namespace ht
{
  /** \addtogroup io
   *  @{
   */

  /** \brief A grabber for parsing the VGM-Dataset
    *
    * The dataset can be found
    * <a href="https://www.iit.it/research/lines/visual-geometry-and-modelling/datasets/514-vgm-6d-pose-of-texture-less-objects-dataset">
    * here</a>.
    * \note Please download all the code and models, and place them in
    * respecting the intended structure explained in the instructions.
    */
  class VgmGrabber : public Grabber
  {
    public:

      //////////////////////////////////////////////////////
      //                  Types
      //////////////////////////////////////////////////////

      /** \brief Signature for the callback function for image
        * \param[in] size_t - frame number
        * \param[in] cv::Mat - an image
        */
      typedef void (cb_vgm_img) ( const size_t,
                                  const cv::Mat&);

      /** \brief Signature for the callback function for image
        * \param[in] size_t - frame number
        * \param[in] Vector4f - rotation (angle axis) from groundtruth
        * \param[in] Vector4f - translation from groundtruth
        */
      typedef void (cb_vgm_motion) (const size_t,
                                    const Vector4f&,
                                    const Vector4f&);

      /** \brief Signature for the callback function of the vgm data set
        * \param[in] size_t - frame number
        * \param[in] cv::Mat - an image
        * \param[in] Vector4f - rotation (angle axis) from groundtruth
        * \param[in] Vector4f - translation from groundtruth
        */
      typedef void (cb_vgm_img_motion) (const size_t,
                                        const cv::Mat&,
                                        const Vector4f&,
                                        const Vector4f&);

      //////////////////////////////////////////////////////
      //                  Methods
      //////////////////////////////////////////////////////

      /** \brief Ctor
        *
        * It requires providing the path to the folder of one of
        * the models
        * \param[in] path - that to the directory of a model e.g.,
        * `<path_to>/vgm/data/Lshape0001/`
        */
      VgmGrabber (const char* const path);

      /** \brief Exposes the camera data used in the dataset
        * acquisition.
        * \return The reference to the Camera object.
        */
      const Camera<double>& getCamera () const { return cam_; }

      /** \brief Exposes the positions of the reference
        * markers used by the VICON system.
        * \note These values are already subtracted of
        * the position of the frame of reference of the
        * platform.
        * \return The reference to the Matrix holding the
        * positions.
        */
      const Matrix<float, 3, Dynamic, ColMajor>&
      getReferencePoints () const { return refs_; }

      bool start () override;

      void stop () override;

      /** \brief Triggers the reading of data */
      void trigger () override;
      
    protected:

      //////////////////////////////////////////////////////
      //                  Types
      //////////////////////////////////////////////////////

      /** \brief Callback flags
        *
        * The are used to determine which types of data
        * are required to be fetched from the data set
        * e.g., if the user doesn't request the images
        * it's not worth to load them.
        */
      enum CbFlags
      {
        /** \brief This flag is set if theres a callback
          * registered which requires the loading of images
          */
        HAS_IMAGE = 0b1u,

        /** \brief This flag is set if theres a callback
          * registered which requires loading and computing
          * the groundtruth motion.
          */
        HAS_MOTION = 0b10u
      };

      //////////////////////////////////////////////////////
      //                  Members
      //////////////////////////////////////////////////////

      /** \brief Store the root folder path for the data set
        * \note Assumes the data set naming conventions were followed
        */
      std::string path_;

      /** \brief Name of the data set
        * \note Assumes the data set naming conventions were followed
        */
      std::string name_;

      /** \brief Camera information extracted from the dataset
        *
        * \note The camera pose corresponds to the transformation from
        * the world coordinates to camera coordinates. Suitable to 
        * perform visualization
        */
      Camera<double> cam_;

      /** \brief The image capturing device */
      cv::VideoCapture vc_;

      /** \brief The file handler responsible for reading the trajectories
        * section of the groundtruth data */
      std::ifstream f_;

      /** \brief Stores the stream position to start reading
        * the trajectories from.
        */
      size_t streampos_;

      /** \brief Stores the reference points coords. It's a dynamic
        * size matrix so it doesn't require the Eigen new operator
        * overload
        */
      Matrix<float, 3, Dynamic, ColMajor> refs_;

      /** \brief Callback bit flag used to determine which types
        * of data fetching are required
        */
      uint8_t cb_flags_;

      //////////////////////////////////////////////////////
      //                  Methods
      //////////////////////////////////////////////////////

      /** \brief Called when the all callbacks are to be notified of
        *  new data
        * \param[in] id - the data frame number
        * \param[in] frame - an image frame
        */
      void cbVgmImg ( const size_t id,
                      const cv::Mat& frame) const;

      /** \brief Called when the all callbacks are to be notified of
        *  new data
        * \param[in] id - the data frame number
        * \param[in] rvec - angleaxis rotation of the object in the
        * VICON reference frame
        * \param[in] tvec - translation of the obj in the VICON
        * reference frame.
        */
      void cbVgmMotion (const size_t id,
                        const Vector4f& rvec,
                        const Vector4f& tvec) const;

      /** \brief Called when the all callbacks are to be notified of
        *  new data
        * \param[in] id - the data frame number
        * \param[in] frame - an image frame
        * \param[in] rvec - angleaxis rotation of the object in the
        * VICON reference frame
        * \param[in] tvec - translation of the obj in the VICON
        * reference frame.
        */
      void cbVgmImgMotion ( const size_t id,
                            const cv::Mat& frame,
                            const Vector4f& rvec,
                            const Vector4f& tvec) const;

      /** \brief Find the stream position to initialize the gt parsing
        * \param[in] id - string to find
        */
      size_t findStreamPos (const char* const id) const;

      /** \brief Initializes the callbacks flags based on the
        * registered signatures
        */
      void initCbFlags ();

      /** \brief Initialized the supported modes on the base class
        * \return Returns the supported modes.
        */
      static std::set<Mode> initSupportedModes ();

      /** \brief Initialized the supported callback signatures
        * \return Returns the supported callback signatures.
        */
      static std::set<const char*> initSupportedSigs ();

      /** \brief Parse camera information
        * \param[in] path - file path to the camera file
        * \return Returns true if parsing is processed successfully, false
        * otherwise
        */
      bool parseCamera (const std::string& path);

      /** \brief Parses the provided configuration file */
      void parseConfigurationFile ();

      /** \brief Responsible for stripping the provided path from any
        * trailing forward slashes and infer the name of the data set
        */
      void parsePathAndName (const std::string& path);

      /** \brief Methods invoked on async mode to continuously fetch
        * data from the data source
        */
      void run ();

    private:

      /** \brief Extracts the sequence origin from a string
        * \param[in] data - the data string
        */
      void parseOriginPoint (const std::string& data);

      /** \brief Extracts the reference points from a string
        * \param[in] data - the data string
        */
      void parseReferencePoints (const std::string& data);


  };

  /** @}*/
}

#endif //CVL_IO_VGM_GRABBER_H_
