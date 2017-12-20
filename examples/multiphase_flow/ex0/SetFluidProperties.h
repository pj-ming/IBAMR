// Filename: SetFluidProperties.h
// Created on Dec 18, 2017 by Nishant Nangia

/////////////////////// INCLUDE GUARD ////////////////////////////////////

#ifndef included_SetFluidProperties
#define included_SetFluidProperties

///////////////////////////// INCLUDES ///////////////////////////////////

#include <ibamr/AdvDiffHierarchyIntegrator.h>
#include <ibtk/ibtk_utilities.h>
#include <tbox/Pointer.h>

namespace IBTK
{
class HierarchyMathOps;
}

/*!
 * Pre processing call back function to be hooked into IBAMR::AdvDiffHierarchyIntegrator class.
 *
 * \param rho_idx a patch data index for the current density variable maintained by the integrator.
 * \param ctx is the pointer to SetFluidProperties class object.
 */

void callSetFluidDensityCallbackFunction(int rho_idx,
                                         SAMRAI::tbox::Pointer<IBTK::HierarchyMathOps> hier_math_ops,
                                         const int integrator_step,
                                         const double current_time,
                                         const bool initial_time,
                                         void* ctx);

/*!
 * Pre processing call back function to be hooked into IBAMR::AdvDiffHierarchyIntegrator class.
 *
 * \param rho_idx a patch data index for the current density variable maintained by the integrator.
 * \param ctx is the pointer to SetFluidProperties class object.
 */

void callSetFluidViscosityCallbackFunction(int mu_idx,
                                           SAMRAI::tbox::Pointer<IBTK::HierarchyMathOps> hier_math_ops,
                                           const int integrator_step,
                                           const double current_time,
                                           const bool initial_time,
                                           void* ctx);

class SetFluidProperties
{
    /*!
     * \brief Class SetFluidProperties is a utility class which sets the fluid and
     * solid Eulerian density based on the current level set information
     */
public:
    /*!
     * The only constructor of this class.
     */
    SetFluidProperties(const std::string& object_name,
                       SAMRAI::tbox::Pointer<IBAMR::AdvDiffHierarchyIntegrator> adv_diff_solver,
                       const std::string& ls_name,
                       const double rho_outside,
                       const double rho_inside,
                       const double mu_outside,
                       const double mu_inside,
                       const int ls_reinit_interval);

    /*!
     * Destructor for this class.
     */
    ~SetFluidProperties();

    /*!
     * Set the density based on the current level set information
     */
    void setDensityPatchData(int rho_idx,
                             SAMRAI::tbox::Pointer<IBTK::HierarchyMathOps> hier_math_ops,
                             const int integrator_step,
                             const double current_time,
                             const bool initial_time);

    /*!
     * Set the viscosity based on the current level set information
     */
    void setViscosityPatchData(int mu_idx,
                               SAMRAI::tbox::Pointer<IBTK::HierarchyMathOps> hier_math_ops,
                               const int integrator_step,
                               const double current_time,
                               const bool initial_time);

    //////////////// PRIVATE /////////////////////////////

private:
    /*!
     * Default constructor is not implemented and should not be used.
     */
    SetFluidProperties();

    /*!
     * Default assignment operator is not implemented and should not be used.
     */
    SetFluidProperties& operator=(const SetFluidProperties& that);

    /*!
     * Default copy constructor is not implemented and should not be used.
     */
    SetFluidProperties(const SetFluidProperties& from);

    /*!
     * Name of this object.
     */
    std::string d_object_name;

    /*!
     * Pointer to advection-diffusion solver.
     */
    SAMRAI::tbox::Pointer<IBAMR::AdvDiffHierarchyIntegrator> d_adv_diff_solver;

    /*!
     * Name of the level set.
     */
    std::string d_ls_name;

    /*!
     * Density of the fluid.
     */
    double d_rho_outside, d_rho_inside;

    /*!
     * Viscosity of the fluid.
     */
    double d_mu_outside, d_mu_inside;

    /*!
     * Level set reinitialization interval
     */
    int d_ls_reinit_interval;

}; // SetFluidProperties

#endif // #ifndef included_SetFluidProperties