/**
  * \author Sergio Agostinho - sergio.r.agostinho@gmail.com
  * \date created: 2017/05/05
  * \date last modified: 2017/05/09
  */
#include <cvl/io/vgm_grabber.h>
#include <cvl/io/utils.h>
#include <cvl/common/geometry.h>
// #include <cvl/common/eigen.h>
#include <tinyxml2.h>
#include <thread>
#include <iostream>

///////////////////////////////////////////////
//                  Public
///////////////////////////////////////////////

ht::VgmGrabber::VgmGrabber (const char* const path)
  : Grabber (initSupportedSigs (), initSupportedModes ())
{
  parsePathAndName (path);
  parseCamera (path_ + '/' + name_ + '/' + name_ + ".xcp");
  parseConfigurationFile ();
  streampos_s_ = findStreamPos ("Segments");
  streampos_t_ = findStreamPos ("Trajectories");
}

bool
ht::VgmGrabber::start ()
{
  // initialize the callbacks flags
  initCbFlags ();

  // Open video capture
  if (cb_flags_ & HAS_IMAGE)
  {
    vc_.open (path_ + '/' + name_ + "/frames/%08d.jpg",  cv::CAP_IMAGES);
    if (!vc_.isOpened ())
    {
      std::cerr << "ERROR: Failed to open video capture device" << std::endl;
      return false;
    }
  }

  // Open text file and seek till after que provided sequence
  if (cb_flags_ & HAS_SEGMENTS)
  {
    f_s_.open (path_ + '/' + name_ + '/' + name_ + ".csv");
    if (!f_s_.is_open ())
    {
      std::cerr << "ERROR: Failed to open groudtruth file" << std::endl;
      stop ();
      return false;
    }
    f_s_.seekg (streampos_s_);
  }

  // Open text file and seek till after que provided trajectories
  if (cb_flags_ & HAS_TRAJECTORIES)
  {
    f_t_.open (path_ + '/' + name_ + '/' + name_ + ".csv");
    if (!f_t_.is_open ())
    {
      std::cerr << "ERROR: Failed to open groudtruth file" << std::endl;
      stop ();
      return false;
    }
    f_t_.seekg (streampos_t_);
  }

  // launch async thread if mode requires
  if (mode_ == Mode::ASYNC)
    thread_ = std::thread (&VgmGrabber::run, this);

  frame_nr_ = 0;
  running_ = true;
  return true;
}

void
ht::VgmGrabber::stop ()
{
  running_ = false;
  if (thread_.joinable ())
    thread_.join();

  if (vc_.isOpened ())
    vc_.release ();

  if (f_s_.is_open ())
    f_s_.close ();

  if (f_t_.is_open ())
    f_t_.close ();
}

void
ht::VgmGrabber::trigger ()
{
  if (!running_)
    return;

  const bool has_segments = cb_flags_ & HAS_SEGMENTS;
  const bool has_image = cb_flags_ & HAS_IMAGE;
  const bool has_trajectories = cb_flags_ & HAS_TRAJECTORIES;

  std::string s_str, t_str;

  if (has_segments && !getline_rn (f_s_, s_str))
  {
    running_ = false;
    return;
  }

  if (has_trajectories && !getline_rn (f_t_, t_str))
  {
    running_ = false;
    return;
  }

  // ignore data from every odd iteration if pulling data
  // from images which also segments/trajectories
  if (has_image && (has_segments || has_trajectories) && (frame_nr_++ % 2))
    return;

  cv::Mat frame;
  if (has_image)
  {
    vc_ >> frame;

    // Check if data is ok
    if (frame.empty ())
    {
      running_ = false;
      return;
    }
  }

  Vector3f euler;
  Vector4f tvec (0.f, 0.f, 0.f, 0.f);
  Vector4f rvec (0.f, 0.f, 0.f, 1.f);
  size_t id = frame_nr_;
  Matrix<float, 3, Dynamic, ColMajor> markers (3, refs_.cols ());

  if (has_segments)
    sscanf (s_str.c_str (),
            "%lu,%*u,%f,%f,%f,%f,%f,%f",
            &id,
            &euler[0], &euler[1], &euler[2],
            &tvec[0], &tvec[1], &tvec[2]);

  if (has_trajectories)
    sscanf (t_str.c_str (),
            "%lu,%*u,"
            "%f,%f,%f,%f,%f,%f,"
            "%f,%f,%f,%f,%f,%f,"
            "%f,%f,%f,%f,%f,%f,"
            "%f,%f,%f,%f,%f,%f",
            &id,
            &markers (0,0), &markers (1,0), &markers (2,0),
            &markers (0,1), &markers (1,1), &markers (2,1),
            &markers (0,2), &markers (1,2), &markers (2,2),
            &markers (0,3), &markers (1,3), &markers (2,3),
            &markers (0,4), &markers (1,4), &markers (2,4),
            &markers (0,5), &markers (1,5), &markers (2,5),
            &markers (0,6), &markers (1,6), &markers (2,6),
            &markers (0,7), &markers (1,7), &markers (2,7));

  // notify
  // rvec = euler_2_angle_axis (euler[0], euler[1], euler[2]);
  rigid (rvec, tvec, markers.transpose (), refs_.transpose ());
  compose (rvec, tvec,
            cam_.rvec.cast<float> (), cam_.tvec.cast<float> (),
            rvec, tvec);
  // rvec = cam_.rvec.cast<float> ();
  // tvec = cam_.tvec.cast<float> ();
  cbVgmImgMotion (id, frame.clone (), rvec, tvec);
  cbVgmRefPoints (id, markers);
}

///////////////////////////////////////////////
//                  Protected
///////////////////////////////////////////////

void
ht::VgmGrabber::cbVgmImgMotion (const size_t id,
                                const cv::Mat& frame,
                                const Vector4f& rvec,
                                const Vector4f& tvec) const
{
  for (const FunctionBase* const f : registered_cbs_.at (typeid (cb_vgm_img_motion).name ()))
    (*static_cast<const Function<cb_vgm_img_motion>*> (f)) (id, frame, rvec, tvec);
}

void
ht::VgmGrabber::cbVgmRefPoints (const size_t id,
                                const Matrix<float, 3, Dynamic, ColMajor>& pts) const
{
  for (const FunctionBase* const f : registered_cbs_.at (typeid (cb_vgm_ref_points).name ()))
    (*static_cast<const Function<cb_vgm_ref_points>*> (f)) (id, pts);
}

size_t
ht::VgmGrabber::findStreamPos (const char* const id) const
{
  std::ifstream ifs (path_ + '/' + name_ + '/' + name_ + ".csv");
  std::string line;
  while (getline_rn (ifs, line))
  {
    if (!line.compare (id))
    {
      // discard four lines
      ifs.ignore (std::numeric_limits<std::streamsize>::max (), '\n');
      ifs.ignore (std::numeric_limits<std::streamsize>::max (), '\n');
      ifs.ignore (std::numeric_limits<std::streamsize>::max (), '\n');
      ifs.ignore (std::numeric_limits<std::streamsize>::max (), '\n');
      return ifs.tellg ();
    }
  }
  return ifs.tellg ();
}

void
ht::VgmGrabber::initCbFlags ()
{
  cb_flags_ = 0u;

  if (!registered_cbs_[typeid (cb_vgm_img_motion).name ()].empty ())
    cb_flags_ |= (HAS_IMAGE | HAS_SEGMENTS);

  if (!registered_cbs_[typeid (cb_vgm_ref_points).name ()].empty ())
    cb_flags_ |= (HAS_TRAJECTORIES);
}

std::set<ht::Grabber::Mode>
ht::VgmGrabber::initSupportedModes ()
{
  static const std::set<Mode> s { Mode::ASYNC, Mode::TRIGGER };
  return s; //copy elision
}

std::set<const char*>
ht::VgmGrabber::initSupportedSigs ()
{
  static const char* const cb_vgm_img_motion_type = typeid (cb_vgm_img_motion).name ();
  static const char* const cb_vgm_ref_points_type = typeid (cb_vgm_ref_points).name ();
  static const std::set<const char*> s { cb_vgm_img_motion_type, cb_vgm_ref_points_type };
  return s; //copy elision
}

bool
ht::VgmGrabber::parseCamera (const std::string& path)
{
  using tinyxml2::XMLDocument;
  using tinyxml2::XMLElement;

  // Load the doc
  XMLDocument doc;
  doc.LoadFile (path.c_str ());
  if (doc.ErrorID ())
  {
    std::cerr << "ERROR: Could not load XML file" << std::endl;
    return false;
  }

  // traverse cameras till we find the one we want
  const XMLElement* el = doc.FirstChildElement ("Cameras")
                              ->LastChildElement ("Camera");
  while (el && strncmp (el->Attribute ("TYPE"), "DCAM", 4))
    el = el->PreviousSiblingElement ("Camera");
  if (!el)
  {
    std::cerr << "ERROR: Could not find correct camera." << std::endl;
    return false;
  }

  // parse image size
  const char* att = el->Attribute ("SENSOR_SIZE");
  if (2 != sscanf (att, "%5u %5u", &cam_.width, &cam_.height))
  {
    std::cerr << "ERROR: Could not parse image size" << std::endl;
    return false;
  }

  // Get a hold of the keyframe element with all the useful info
  el = el->LastChildElement ("KeyFrames")->FirstChildElement ("KeyFrame");
  if (!el)
  {
    std::cerr << "ERROR: Failed to find camera info." << std::endl;
    return false;
  }

  // Parse intrinsics matrix
  cam_.k = Matrix43d::Identity ();
  cam_.k(0, 0) = cam_.k(1, 1) = el->DoubleAttribute ("FOCAL_LENGTH");
  att = el->Attribute ("PRINCIPAL_POINT");
  int n = sscanf (att, "%20lf %20lf", &cam_.k(0, 2), &cam_.k(1, 2));
  if (n < 2)
  {
    std::cerr << "ERROR: Failed to parse camera's intrinsics."
              << std::endl;
    return false;
  }

  // Parse Extrinsics - original data is Cam to W 
  Quaterniond q;
  att = el->Attribute ("ORIENTATION");
  n = sscanf (att, "%20lf %20lf %20lf %20lf", &q.x (), &q.y (), &q.z (), &q.w ());

  Vector4d t (0, 0, 0, 0);
  att = el->Attribute ("POSITION");
  n += sscanf (att, "%20lf %20lf %20lf", &t[0], &t[1], &t[2]);
  if (n < 7)
  {
    std::cerr << "ERROR: Failed to parse the camera's extrinsics"
              << std::endl;
    return false;
  }
  
  // Perform final conversion to storage world to camera
  cam_.rvec = angle_axis (q);
  // cam_.tvec = t;
  // cam_.rvec[0] *= -1;
  cam_.tvec = -rotate4 (cam_.rvec, t);
  return true;
}

void
ht::VgmGrabber::parseConfigurationFile ()
{
  //utils
  const size_t npos = std::string::npos;

  std::ifstream ifs (path_ + "/../code/conf/" + name_ + ".m");
  if (!ifs.is_open ())
  {
    std::cerr << "Failed to open the configuration file" << std::endl;
    return;
  }

  bool parsing_refs = false, parsing_origin = false;
  const std::string refs_str ("seq.ReferencePoints"),
                    origin_str ("seq.Origin");

  // Go line by line
  size_t beg = npos, end = npos;
  for (std::string line, data_str; std::getline (ifs, line);)
  {
    //ignore empty and comment lines
    size_t pos = line.find_first_not_of (" \t");
    if (pos == npos || line[pos] == '%')
      continue;

    if (!parsing_refs && !parsing_origin)
    {
      pos = line.find (refs_str);
      if (pos != npos)
      {
        parsing_refs = true;
        data_str.clear ();
      }
    }

    if (!parsing_refs && !parsing_origin)
    {
      pos = line.find (origin_str);
      if (pos != npos)
      {
        parsing_origin = true;
        data_str.clear ();
      }
    }

    if (parsing_refs)
    {
      if (beg == npos)
      {
        beg = line.find("[[");
        if (beg != npos) ++beg;
      }
      else
        beg = 0;

      end = line.find ("]]");
      const size_t length = (end == npos)? npos : end - beg + 1;
      data_str += line.substr (beg, length) + '\n';
      if (end != npos)
      {
        parseReferencePoints (data_str);
        parsing_refs = false;
        beg = end = npos;
        data_str.clear ();
      }
    }

    if (parsing_origin)
    {
      beg = line.find('[') + 1;
      end = line.find(']');
      parseOriginPoint (line.substr (beg, end - beg));
      parsing_origin = false;
      beg = end = npos;
    }
  }
}

void
ht::VgmGrabber::parsePathAndName (const std::string& path)
{
  size_t upper = path.size ();
  size_t lower = path.find_last_of ("/\\");

  while (!path.substr (lower, upper - lower - 1).size ())
  {
    upper = lower;
    lower = path.find_last_of ("/\\", lower - 1);
  }

  // Store result in class
  path_ = path.substr (0, lower);
  name_ = path.substr (lower + 1, upper - lower - 1);
}

void
ht::VgmGrabber::run ()
{
  while (running_)
    trigger ();
}

///////////////////////////////////////////////
//                  Private
///////////////////////////////////////////////

void
ht::VgmGrabber::parseOriginPoint (const std::string& data)
{
  Vector3f origin (0.f, 0.f, 0.f);

  std::istringstream ss (data);
  std::string number;
  std::getline (ss, number, ';');
  origin[0] = std::stof (number);
  std::getline (ss, number, ';');
  origin[1] = std::stof (number);
  std::getline (ss, number);
  origin[2] = std::stof (number);

  refs_ = refs_.colwise () - origin;
}

void
ht::VgmGrabber::parseReferencePoints (const std::string& data)
{
  std::vector<float> pts;
  std::istringstream ss (data);
  for (std::string line; std::getline (ss, line);)
  {
    const size_t beg = line.find ('[') + 1;
    const size_t end = line.find (']', beg);
    std::istringstream ss_line (line.substr (beg, end - beg));

    std::string number;
    std::getline (ss_line, number, ';');
    pts.emplace_back (std::stof (number));
    std::getline (ss_line, number, ';');
    pts.emplace_back (std::stof (number));
    std::getline (ss_line, number);
    pts.emplace_back (std::stof (number));
  }

  assert (!(pts.size () % 3));
  refs_ = Eigen::Map<Matrix<float, 3, Dynamic, ColMajor>> ( pts.data (),
                                                            3,
                                                            pts.size () / 3);
}
