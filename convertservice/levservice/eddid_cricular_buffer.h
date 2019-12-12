#pragma once
/*********************************************************************************************************************
*** file: cricular_buffer.h
*** auth: create by xiay
*** desc: implement ring buffer
**********************************************************************************************************************/
#include <mutex>
#include <memory>

namespace eddid {
    namespace event_wrap {
        namespace eutil {
            constexpr uint32_t def_cricular_buf_size = 10;

            template<typename T>
            class CricularBuffer {
            public:
                CricularBuffer(const CricularBuffer &) = delete;

                CricularBuffer &operator=(const CricularBuffer &) = delete;

            public:
                CricularBuffer(uint32_t buffer_size = def_cricular_buf_size);

                ~CricularBuffer();

            public:
                bool push(T &&t);

                T &front();

                void pop();

                T &back();

                void resize(uint32_t size);

                uint32_t buffer_size() const { return buffer_size_; }

                size_t size() const { return data_size_; }

            private:
                typedef struct __tag_ST_LINK_NODE {
                    std::shared_ptr <__tag_ST_LINK_NODE> next;
                    T data;

                    __tag_ST_LINK_NODE() = default;

                    ~__tag_ST_LINK_NODE() = default;
                } ST_LINK_NODE, *LPST_LINK_NODE;
                typedef std::shared_ptr <ST_LINK_NODE> LinkNodePtr;
                typedef std::shared_ptr<const ST_LINK_NODE> LinkNodeCPtr;
            private:
                std::uint32_t buffer_size_;
                std::uint32_t data_size_;
                std::uint32_t free_node_num_;
                LinkNodePtr data_lst_ptr_;
                LinkNodePtr free_lst_ptr_;
                std::mutex lst_spin_lock_;

                std::weak_ptr <ST_LINK_NODE> current_insert_node_ptr_;
                std::weak_ptr <ST_LINK_NODE> current_lst_end_ptr_;
                std::weak_ptr <ST_LINK_NODE> current_back_data_ptr_;
            };

            template<typename T>
            eutil::CricularBuffer<T>::CricularBuffer(uint32_t buffer_size)
                    : buffer_size_(buffer_size), data_size_(0), free_node_num_(0) {
                free_node_num_ = buffer_size + 1;
                free_lst_ptr_ = std::make_shared<ST_LINK_NODE>();
                std::weak_ptr <ST_LINK_NODE> free_node_ptr(free_lst_ptr_);
                for (uint32_t i = 0; i < buffer_size; ++i) {
                    LinkNodePtr ptr = std::make_shared<ST_LINK_NODE>();
                    free_node_ptr.lock()->next = ptr;
                    free_node_ptr = ptr;
                }
            }

            template<typename T>
            eutil::CricularBuffer<T>::~CricularBuffer() {
                while (data_lst_ptr_) {
                    LinkNodePtr current_node_ptr = data_lst_ptr_;
                    data_lst_ptr_ = current_node_ptr->next;
                    current_node_ptr->next = nullptr;
                }

                while (free_lst_ptr_) {
                    LinkNodePtr current_node_ptr = free_lst_ptr_;
                    free_lst_ptr_ = current_node_ptr->next;
                    current_node_ptr->next = nullptr;
                }
            }

            template<typename T>
            bool eutil::CricularBuffer<T>::push(T &&t) {
                std::lock_guard <std::mutex> locker(lst_spin_lock_);
                LinkNodePtr insert_node_ptr = free_lst_ptr_;
                free_lst_ptr_ = free_lst_ptr_->next;
                insert_node_ptr->data = t;
                insert_node_ptr->next = nullptr;
                free_node_num_--;
                if (!data_lst_ptr_) {
                    data_size_++;
                    data_lst_ptr_ = insert_node_ptr;
                    current_lst_end_ptr_ = data_lst_ptr_;
                    current_insert_node_ptr_ = data_lst_ptr_;
                    current_back_data_ptr_ = data_lst_ptr_;
                } else {
                    current_insert_node_ptr_.lock()->next = insert_node_ptr;
                    current_insert_node_ptr_ = insert_node_ptr;
                    current_lst_end_ptr_ = insert_node_ptr;
                    current_back_data_ptr_ = insert_node_ptr;
                    if (data_size_ >= buffer_size_) {
                        insert_node_ptr = data_lst_ptr_;
                        data_lst_ptr_ = data_lst_ptr_->next;

                        insert_node_ptr->data = T();
                        insert_node_ptr->next = free_lst_ptr_;
                        free_lst_ptr_ = insert_node_ptr;
                        free_node_num_++;
                    } else {
                        data_size_++;
                    }
                }
                return true;
            }

            template<typename T>
            T &eutil::CricularBuffer<T>::front() {
                if (data_lst_ptr_) {
                    return &(data_lst_ptr_()->data);
                }
                return nullptr;
            }

            template<typename T>
            void eutil::CricularBuffer<T>::pop() {
                if (data_size_ == 0) {
                    return;
                }
                std::lock_guard <std::mutex> locker(lst_spin_lock_);
                LinkNodePtr current_node_ptr = data_lst_ptr_;
                data_lst_ptr_ = current_node_ptr->next;
                current_node_ptr->next = nullptr;
                data_size_--;

                current_node_ptr->next = free_lst_ptr_;
                free_lst_ptr_ = current_node_ptr;
                free_node_num_++;
            }

            template<typename T>
            T &eutil::CricularBuffer<T>::back() {
                return current_back_data_ptr_.lock()->data;
            }

            template<typename T>
            void eutil::CricularBuffer<T>::resize(uint32_t size) {
                std::lock_guard <std::mutex> locker(lst_spin_lock_);

                if (size <= buffer_size_) {
                    if (data_size_ <= size) {
                        uint32_t sum = free_node_num_ + data_size_ - size - 1;
                        free_node_num_ = size - data_size_ + 1;
                        for (uint32_t free_node = 0; free_node < sum; ++free_node) {
                            LinkNodePtr current_node_ptr = free_lst_ptr_;
                            free_lst_ptr_ = current_node_ptr->next;
                            current_node_ptr->next = nullptr;
                        }
                    } else {
                        uint32_t sum = data_size_ - size;
                        data_size_ = sum;
                        for (uint32_t free_node = 0; free_node < sum; ++free_node) {
                            LinkNodePtr current_node_ptr = data_lst_ptr_;
                            data_lst_ptr_ = current_node_ptr->next;
                            current_node_ptr->next = nullptr;
                        }

                        sum = free_node_num_ - 1;
                        for (uint32_t free_node = 0; free_node < sum; ++free_node) {
                            LinkNodePtr current_node_ptr = free_lst_ptr_;
                            free_lst_ptr_ = current_node_ptr->next;
                            current_node_ptr->next = nullptr;
                        }
                    }

                    buffer_size_ = size;
                    return;
                }

                uint32_t sum = size - data_size_ - free_node_num_ + 1;
                buffer_size_ = size;
                for (uint32_t free_node = 0; free_node < sum; ++free_node) {
                    LinkNodePtr current_node_ptr = std::make_shared<ST_LINK_NODE>();;
                    current_node_ptr->next = free_lst_ptr_;
                    free_lst_ptr_ = current_node_ptr;
                    free_node_num_++;
                }
            }
        } // end of namespace eutil
    } // end of namespace ngp
}
