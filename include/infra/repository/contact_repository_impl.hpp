#ifndef __IM_INFRA_REPOSITORY_CONTACT_REPOSITORY_IMPL_HPP__
#define __IM_INFRA_REPOSITORY_CONTACT_REPOSITORY_IMPL_HPP__

#include "db/mysql.hpp"
#include "domain/repository/contact_repository.hpp"

namespace IM::infra::repository {

class ContactRepositoryImpl : public IM::domain::repository::IContactRepository {
   public:
    explicit ContactRepositoryImpl(std::shared_ptr<IM::MySQLManager> db_manager);
    bool GetContactItemListByUserId(const uint64_t user_id, std::vector<dto::ContactItem>& out,
                                    std::string* err = nullptr) override;
    bool GetByOwnerAndTarget(const uint64_t owner_id, const uint64_t target_id,
                             dto::ContactDetails& out, std::string* err = nullptr) override;
    bool GetByOwnerAndTarget(const std::shared_ptr<IM::MySQL>& db, const uint64_t owner_id,
                             const uint64_t target_id, dto::ContactDetails& out,
                             std::string* err = nullptr) override;
    bool UpsertContact(const std::shared_ptr<IM::MySQL>& db, const model::Contact& c,
                       std::string* err = nullptr) override;
    bool EditRemark(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                    const uint64_t contact_id, const std::string& remark,
                    std::string* err = nullptr) override;
    bool DeleteContact(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                       const uint64_t contact_id, std::string* err = nullptr) override;
    bool UpdateStatusAndRelation(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                 const uint64_t contact_id, const uint8_t status,
                                 const uint8_t relation, std::string* err = nullptr) override;
    bool ChangeContactGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                            const uint64_t contact_id, const uint64_t group_id,
                            std::string* err = nullptr) override;
    bool GetOldGroupId(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                       const uint64_t contact_id, uint64_t& out_group_id,
                       std::string* err = nullptr) override;
    bool RemoveFromGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                         const uint64_t contact_id, std::string* err = nullptr) override;
    bool RemoveFromGroupByGroupId(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                                  const uint64_t group_id, std::string* err = nullptr) override;
    bool CreateContactApply(const model::ContactApply& a, std::string* err = nullptr) override;
    bool GetPendingCountById(const uint64_t id, uint64_t& out_count,
                             std::string* err = nullptr) override;
    bool GetContactApplyItemById(const uint64_t id, std::vector<dto::ContactApplyItem>& out,
                                 std::string* err = nullptr) override;
    bool AgreeApply(const std::shared_ptr<IM::MySQL>& db, const uint64_t user_id,
                    const uint64_t apply_id, const std::string& remark,
                    std::string* err = nullptr) override;
    bool RejectApply(const uint64_t handler_user_id, const uint64_t apply_user_id,
                     const std::string& remark, std::string* err = nullptr) override;
    bool GetDetailById(const std::shared_ptr<IM::MySQL>& db, const uint64_t apply_id,
                       model::ContactApply& out, std::string* err = nullptr) override;
    bool GetDetailById(const uint64_t apply_id, model::ContactApply& out,
                       std::string* err = nullptr) override;
    bool CreateContactGroup(const std::shared_ptr<IM::MySQL>& db, const model::ContactGroup& g,
                            uint64_t& out_id, std::string* err = nullptr) override;
    bool UpdateContactGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t id,
                            const uint32_t sort, const std::string& name,
                            std::string* err = nullptr) override;
    bool GetContactGroupItemListByUserId(const uint64_t user_id,
                                         std::vector<dto::ContactGroupItem>& outs,
                                         std::string* err = nullptr) override;
    bool GetContactGroupItemListByUserId(const std::shared_ptr<IM::MySQL>& db,
                                         const uint64_t user_id,
                                         std::vector<dto::ContactGroupItem>& outs,
                                         std::string* err = nullptr) override;
    bool DeleteContactGroup(const std::shared_ptr<IM::MySQL>& db, const uint64_t id,
                            std::string* err = nullptr) override;
    bool UpdateContactCount(const std::shared_ptr<IM::MySQL>& db, const uint64_t group_id,
                            bool increase, std::string* err = nullptr) override;

   private:
    std::shared_ptr<IM::MySQLManager> m_db_manager;
};

}  // namespace IM::infra::repository

#endif  // __IM_INFRA_REPOSITORY_CONTACT_REPOSITORY_IMPL_HPP__