#include <vector>

#include "caffe/layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/vision_layers.hpp"

namespace caffe {

template <typename Dtype>
void BiasChannelLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  BiasChannelParameter param = this->layer_param_.bias_channel_param();
  bg_bias_ = param.bg_bias();
  fg_bias_ = param.fg_bias();
  CHECK_GE(bg_bias_, 0) << "BG bias needs to be positive";
  CHECK_GE(fg_bias_, 0) << "FG bias needs to be positive";
  // -1 is just a filler to make sure that the length of label list equals max_labels_
  ignore_label_.insert(-1);
  for (int i = 0; i < param.ignore_label_size(); ++i){
    ignore_label_.insert(param.ignore_label(i));
  }
}

template <typename Dtype>
void BiasChannelLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  const BiasChannelParameter_LabelType label_type =
    this->layer_param_.bias_channel_param().label_type();
  num_ = bottom[0]->num();
  channels_ = bottom[0]->channels();
  height_ = bottom[0]->height();
  width_ = bottom[0]->width();
  //
  CHECK_EQ(bottom[1]->num(), num_) << "Input channels incompatible in num";
  max_labels_ = bottom[1]->channels();
  CHECK_GE(max_labels_, 1) << "Label blob needs to be non-empty";
  switch (label_type) {
  case BiasChannelParameter_LabelType_IMAGE:
    CHECK_EQ(bottom[1]->height(), 1) << "Label height";
    CHECK_EQ(bottom[1]->width(), 1) << "Label width";
    break;
  case BiasChannelParameter_LabelType_PIXEL:
    CHECK_EQ(bottom[1]->channels(), 1) << "Label channels";
    CHECK_EQ(bottom[1]->height(), height_) << "Label height";
    CHECK_EQ(bottom[1]->width(), width_) << "Label width";
    break;
  default:
    LOG(FATAL) << "Unexpected label type";
  }
  //
  top[0]->Reshape(num_, channels_, height_, width_);
}

template <typename Dtype>
void BiasChannelLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  const BiasChannelParameter_LabelType label_type =
    this->layer_param_.bias_channel_param().label_type();
  caffe_copy(bottom[0]->count(), bottom[0]->cpu_data(), top[0]->mutable_cpu_data());
  for (int n = 0; n < num_; ++n) {
    if (label_type == BiasChannelParameter_LabelType_IMAGE) {
      for (int j = 0; j < max_labels_; ++j) {
	const int label = static_cast<int>(*bottom[1]->cpu_data(n, j));
	if (ignore_label_.count(label) != 0) {
	  continue;
	} else if (label >= 0 && label < channels_) {
	  // Bias the foreground or background scores
	  const Dtype bias = (label == 0) ? bg_bias_ : fg_bias_;
	  caffe_add_scalar(height_ * width_, bias, top[0]->mutable_cpu_data(n, label));	
	} else {
	  LOG(FATAL) << "Unexpected label " << label;
	}
      }
    }
    else if (label_type == BiasChannelParameter_LabelType_PIXEL) {
      const Dtype *label_data = bottom[1]->cpu_data(n);
      Dtype *top_data = top[0]->mutable_cpu_data(n);
      const int spatial_dim = height_ * width_;
      for (int j = 0; j < spatial_dim; ++j) {
	const int label = static_cast<int>(label_data[j]);
	if (ignore_label_.count(label) != 0) {
	  continue;
	} else if (label == 0) {
	  top_data[j] += bg_bias_;
	} else if (label > 0 && label < channels_) {
	  // Bias the foreground and background scores
	  top_data[j] += bg_bias_;
	  top_data[label * spatial_dim + j] += fg_bias_;
	} else {
	  LOG(FATAL) << "Unexpected label " << label;
	}
      }
    }
  }
}

template <typename Dtype>
void BiasChannelLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[1]) {
    LOG(FATAL) << "Cannot propagate down to label input";
  }
  if (propagate_down[0]) {
    caffe_copy(bottom[0]->count(), top[0]->cpu_diff(), bottom[0]->mutable_cpu_diff());
  }
}

#ifndef CPU_ONLY
template <typename Dtype>
void BiasChannelLayer<Dtype>::Forward_gpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  // TODO(gpapan): write a CUDA kernel for this case
  const BiasChannelParameter_LabelType label_type =
    this->layer_param_.bias_channel_param().label_type();
  if (label_type == BiasChannelParameter_LabelType_PIXEL) {
    Forward_cpu(bottom, top);
    return;
  }
  caffe_copy(bottom[0]->count(), bottom[0]->gpu_data(), top[0]->mutable_gpu_data());
  for (int n = 0; n < num_; ++n) {
    for (int j = 0; j < max_labels_; ++j) {
      const int label = static_cast<int>(*bottom[1]->cpu_data(n, j));
      if (ignore_label_.count(label) != 0) {
	continue;
      } else if (label >= 0 && label < channels_) {
	// Bias the foreground or background scores
	const Dtype bias = (label == 0) ? bg_bias_ : fg_bias_;
	caffe_gpu_add_scalar(height_ * width_, bias, top[0]->mutable_gpu_data(n, label));	
      } else {
	LOG(FATAL) << "Unexpected label " << label;
      }
    }
  }
}


template <typename Dtype>
void BiasChannelLayer<Dtype>::Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[1]) {
    LOG(FATAL) << "Cannot propagate down to label input";
  }
  if (propagate_down[0]) {
    caffe_copy(bottom[0]->count(), top[0]->gpu_diff(), bottom[0]->mutable_gpu_diff());
  }
}
#endif

#ifdef CPU_ONLY
STUB_GPU(BiasChannelLayer);
#endif

INSTANTIATE_CLASS(BiasChannelLayer);
REGISTER_LAYER_CLASS(BIAS_CHANNEL, BiasChannelLayer);

}  // namespace caffe
